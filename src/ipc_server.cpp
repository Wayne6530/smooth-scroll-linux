// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Wayne6530

#include "ipc_server.h"

#include <algorithm>
#include <cassert>
#include <limits>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>
#include <cstring>
#include <cerrno>

#include <spdlog/spdlog.h>

namespace smooth_scroll
{

IpcServer::IpcServer(std::string_view shm_name) : shm_name_(shm_name)
{
}

IpcServer::~IpcServer()
{
  cleanup();
}

bool IpcServer::initialize()
{
  shm_fd_ = shm_open(shm_name_.c_str(), O_RDWR | O_CREAT, 0666);
  if (shm_fd_ == -1)
  {
    SPDLOG_ERROR("Failed to open shared memory '{}': {}", shm_name_, std::strerror(errno));
    return false;
  }

  if (fchmod(shm_fd_, 0666) == -1)
  {
    SPDLOG_ERROR("Failed to fchmod shared memory: {}", std::strerror(errno));
    close(shm_fd_);
    shm_fd_ = -1;
    return false;
  }

  if (ftruncate(shm_fd_, sizeof(SmoothScrollIPC)) == -1)
  {
    SPDLOG_ERROR("Failed to truncate shared memory: {}", std::strerror(errno));
    close(shm_fd_);
    shm_fd_ = -1;
    return false;
  }

  void* addr = mmap(nullptr, sizeof(SmoothScrollIPC), PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd_, 0);
  if (addr == MAP_FAILED)
  {
    SPDLOG_ERROR("Failed to mmap shared memory: {}", std::strerror(errno));
    close(shm_fd_);
    shm_fd_ = -1;
    return false;
  }

  mapped_memory_ = static_cast<SmoothScrollIPC*>(addr);

  mapped_memory_->state_bits.store(0, std::memory_order_relaxed);
  mapped_memory_->scroll_id.store(0, std::memory_order_relaxed);
  mapped_memory_->force_passthrough.store(0, std::memory_order_relaxed);
  mapped_memory_->auto_scroll_offset.store(0, std::memory_order_relaxed);
  mapped_memory_->reserved[0].store(0, std::memory_order_relaxed);
  mapped_memory_->reserved[1].store(0, std::memory_order_relaxed);

  mapped_memory_->daemon_pid.store(getpid(), std::memory_order_relaxed);
  mapped_memory_->magic_version.store(IPC_MAGIC_VERSION_EXPECTED, std::memory_order_release);

  scroll_id_ = 0;
  force_passthrough_enabled_ = false;

  return true;
}

void IpcServer::cleanup() noexcept
{
  if (mapped_memory_)
  {
    mapped_memory_->daemon_pid.store(0, std::memory_order_relaxed);
    mapped_memory_->state_bits.store(0, std::memory_order_relaxed);
    mapped_memory_->auto_scroll_offset.store(0, std::memory_order_relaxed);

    munmap(mapped_memory_, sizeof(SmoothScrollIPC));
    mapped_memory_ = nullptr;
  }

  if (shm_fd_ != -1)
  {
    close(shm_fd_);
    shm_fd_ = -1;
  }
}

void IpcServer::setConnected() noexcept
{
  state_ |= IPC_STATE_CONNECTED;
  mapped_memory_->state_bits.store(state_, std::memory_order_relaxed);
}

void IpcServer::setDisconnected() noexcept
{
  state_ = 0;
  mapped_memory_->state_bits.store(0, std::memory_order_relaxed);
  mapped_memory_->auto_scroll_offset.store(0, std::memory_order_relaxed);
}

void IpcServer::setPassthrough(bool passthrough) noexcept
{
  if (passthrough)
  {
    state_ |= IPC_STATE_PASSTHROUGH;
  }
  else
  {
    state_ &= ~IPC_STATE_PASSTHROUGH;
  }
  mapped_memory_->state_bits.store(state_, std::memory_order_relaxed);
}

void IpcServer::setDragView(bool drag_view) noexcept
{
  if (drag_view)
  {
    state_ |= IPC_STATE_DRAG_VIEW;
    state_ &= 0x0000FFFF;
  }
  else
  {
    state_ &= ~IPC_STATE_DRAG_VIEW;
  }
  mapped_memory_->state_bits.store(state_, std::memory_order_relaxed);
}

void IpcServer::setFreeSpin(bool free_spin) noexcept
{
  if (free_spin)
  {
    state_ |= IPC_STATE_FREE_SPIN;
  }
  else
  {
    state_ &= ~IPC_STATE_FREE_SPIN;
  }
  mapped_memory_->state_bits.store(state_, std::memory_order_relaxed);
}

void IpcServer::setAutoScroll(bool auto_scroll) noexcept
{
  if (auto_scroll)
  {
    state_ |= IPC_STATE_AUTO_SCROLL;
  }
  else
  {
    state_ &= ~IPC_STATE_AUTO_SCROLL;
  }
  mapped_memory_->state_bits.store(state_, std::memory_order_relaxed);
}

void IpcServer::setAutoScrollAxes(bool horizontal_enabled, bool vertical_enabled) noexcept
{
  state_ &= ~(IPC_STATE_AUTO_SCROLL_HORIZONTAL_ENABLED | IPC_STATE_AUTO_SCROLL_VERTICAL_ENABLED);
  if (horizontal_enabled)
    state_ |= IPC_STATE_AUTO_SCROLL_HORIZONTAL_ENABLED;
  if (vertical_enabled)
    state_ |= IPC_STATE_AUTO_SCROLL_VERTICAL_ENABLED;
  mapped_memory_->state_bits.store(state_, std::memory_order_relaxed);
}

void IpcServer::setAutoScrollOffset(int64_t horizontal, int64_t vertical) noexcept
{
  const int16_t clamped_horizontal = static_cast<int16_t>(
      std::clamp<int64_t>(horizontal, std::numeric_limits<int16_t>::min(), std::numeric_limits<int16_t>::max()));
  const int16_t clamped_vertical = static_cast<int16_t>(
      std::clamp<int64_t>(vertical, std::numeric_limits<int16_t>::min(), std::numeric_limits<int16_t>::max()));
  const uint32_t packed = packAutoScrollOffset(clamped_horizontal, clamped_vertical);
  mapped_memory_->auto_scroll_offset.store(packed, std::memory_order_relaxed);
}

void IpcServer::setSpeed(double speed, bool positive, bool horizontal) noexcept
{
  uint32_t clamped_speed = static_cast<uint32_t>(std::clamp(speed, 0.0, 65535.0));

  state_ &= ~(0xFFFF0000u | IPC_STATE_HORIZONTAL | IPC_STATE_DIRECTION);

  if (horizontal)
    state_ |= IPC_STATE_HORIZONTAL;

  if (positive)
    state_ |= IPC_STATE_DIRECTION;

  state_ |= (clamped_speed << IPC_STATE_SPEED_SHIFT);
  mapped_memory_->state_bits.store(state_, std::memory_order_relaxed);
}

void IpcServer::resetMotionState() noexcept
{
  state_ &= ~(0xFFFF0000u | IPC_STATE_HORIZONTAL | IPC_STATE_DIRECTION | IPC_STATE_DRAG_VIEW | IPC_STATE_FREE_SPIN |
              IPC_STATE_AUTO_SCROLL);
  mapped_memory_->state_bits.store(state_, std::memory_order_relaxed);
  mapped_memory_->auto_scroll_offset.store(0, std::memory_order_relaxed);
}

[[nodiscard]] bool IpcServer::checkBrakeRequest() noexcept
{
  assert(mapped_memory_);

  bool brake = false;
  uint32_t current_id = mapped_memory_->scroll_id.load(std::memory_order_relaxed);
  if (current_id != scroll_id_)
  {
    scroll_id_ = current_id;
    brake = true;
  }

  const bool force_passthrough = isForcePassthroughEnabled();
  if (force_passthrough && !force_passthrough_enabled_)
  {
    brake = true;
  }
  force_passthrough_enabled_ = force_passthrough;

  return brake;
}

[[nodiscard]] bool IpcServer::isForcePassthroughEnabled() const noexcept
{
  assert(mapped_memory_);

  return mapped_memory_->force_passthrough.load(std::memory_order_relaxed) > 0;
}

}  // namespace smooth_scroll
