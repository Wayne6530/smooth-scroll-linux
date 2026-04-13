#include "ipc_server.h"

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
  mapped_memory_->reserved[0].store(0, std::memory_order_relaxed);
  mapped_memory_->reserved[1].store(0, std::memory_order_relaxed);
  mapped_memory_->reserved[2].store(0, std::memory_order_relaxed);

  mapped_memory_->daemon_pid.store(getpid(), std::memory_order_relaxed);
  mapped_memory_->magic_version.store(MAGIC_VERSION_EXPECTED, std::memory_order_release);

  scroll_id_ = 0;

  return true;
}

void IpcServer::cleanup() noexcept
{
  if (mapped_memory_)
  {
    mapped_memory_->daemon_pid.store(0, std::memory_order_relaxed);
    mapped_memory_->state_bits.store(0, std::memory_order_relaxed);

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
  state_ |= (1 << 0);
  mapped_memory_->state_bits.store(state_, std::memory_order_relaxed);
}

void IpcServer::setPassthrough(bool passthrough) noexcept
{
  if (passthrough)
  {
    state_ |= (1 << 1);
  }
  else
  {
    state_ &= ~(1 << 1);
  }
  mapped_memory_->state_bits.store(state_, std::memory_order_relaxed);
}

void IpcServer::setDragView(bool drag_view) noexcept
{
  if (drag_view)
  {
    state_ |= (1 << 2);
    state_ &= 0x0000FFFF;
  }
  else
  {
    state_ &= ~(1 << 2);
  }
  mapped_memory_->state_bits.store(state_, std::memory_order_relaxed);
}

void IpcServer::setFreeSpin(bool free_spin) noexcept
{
  if (free_spin)
  {
    state_ |= (1 << 3);
  }
  else
  {
    state_ &= ~(1 << 3);
  }
  mapped_memory_->state_bits.store(state_, std::memory_order_relaxed);
}

void IpcServer::setSpeed(double speed, bool positive, bool horizontal) noexcept
{
  uint32_t clamped_speed = static_cast<uint32_t>(std::clamp(speed, 0.0, 65535.0));

  state_ &= 0x0000FFCF;

  if (horizontal)
    state_ |= (1 << 4);

  if (positive)
    state_ |= (1 << 5);

  state_ |= (clamped_speed << 16);
  mapped_memory_->state_bits.store(state_, std::memory_order_relaxed);
}

[[nodiscard]] bool IpcServer::checkBrakeSignal() noexcept
{
  assert(mapped_memory_);

  uint32_t current_id = mapped_memory_->scroll_id.load(std::memory_order_relaxed);
  if (current_id != scroll_id_)
  {
    scroll_id_ = current_id;
    return true;
  }

  return false;
}

[[nodiscard]] bool IpcServer::isForcePassthroughEnabled() const noexcept
{
  assert(mapped_memory_);

  return mapped_memory_->force_passthrough.load(std::memory_order_relaxed) > 0;
}

}  // namespace smooth_scroll
