// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Wayne6530

#include "ipc_client.h"

#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>

namespace SmoothScrollKWin
{

IpcClient::~IpcClient()
{
  close();
}

IpcSnapshot IpcClient::readSnapshot()
{
  if (!connect() || !isMappedValid())
  {
    close();
    return invalidSnapshot();
  }

  const uint32_t pid = m_ipc->daemon_pid.load(std::memory_order_relaxed);
  if (pid == 0)
  {
    return invalidSnapshot();
  }

  const uint32_t stateBits = m_ipc->state_bits.load(std::memory_order_relaxed);
  const uint32_t autoScrollOffset = m_ipc->auto_scroll_offset.load(std::memory_order_relaxed);

  IpcSnapshot snapshot;
  snapshot.valid = true;
  snapshot.pid = pid;
  snapshot.scrollId = m_ipc->scroll_id.load(std::memory_order_relaxed);
  snapshot.forcePassthrough = m_ipc->force_passthrough.load(std::memory_order_relaxed);
  snapshot.connected = (stateBits & smooth_scroll::IPC_STATE_CONNECTED) != 0;
  snapshot.passthrough = (stateBits & smooth_scroll::IPC_STATE_PASSTHROUGH) != 0;
  snapshot.dragView = (stateBits & smooth_scroll::IPC_STATE_DRAG_VIEW) != 0;
  snapshot.freeSpin = (stateBits & smooth_scroll::IPC_STATE_FREE_SPIN) != 0;
  snapshot.autoScroll = (stateBits & smooth_scroll::IPC_STATE_AUTO_SCROLL) != 0;
  snapshot.autoScrollHorizontalEnabled =
      (stateBits & smooth_scroll::IPC_STATE_AUTO_SCROLL_HORIZONTAL_ENABLED) != 0;
  snapshot.autoScrollVerticalEnabled =
      (stateBits & smooth_scroll::IPC_STATE_AUTO_SCROLL_VERTICAL_ENABLED) != 0;
  snapshot.autoScrollOffsetX = smooth_scroll::autoScrollHorizontalOffset(autoScrollOffset);
  snapshot.autoScrollOffsetY = smooth_scroll::autoScrollVerticalOffset(autoScrollOffset);
  snapshot.horizontal = (stateBits & smooth_scroll::IPC_STATE_HORIZONTAL) != 0;
  snapshot.positive = (stateBits & smooth_scroll::IPC_STATE_DIRECTION) != 0;
  snapshot.speed = stateBits >> smooth_scroll::IPC_STATE_SPEED_SHIFT;
  return snapshot;
}

bool IpcClient::requestStop(const IpcSnapshot& snapshot)
{
  if (!snapshot.valid && !isMappedValid())
  {
    return false;
  }

  const uint32_t current = snapshot.valid ? snapshot.scrollId : m_ipc->scroll_id.load(std::memory_order_relaxed);
  m_ipc->scroll_id.store(current + 1, std::memory_order_relaxed);
  return true;
}

bool IpcClient::setForcePassthrough(bool enabled)
{
  if (!connect() || !isMappedValid())
  {
    close();
    return false;
  }

  m_ipc->force_passthrough.store(enabled ? 1u : 0u, std::memory_order_relaxed);
  return true;
}

void IpcClient::close()
{
  if (m_ipc)
  {
    munmap(m_ipc, sizeof(smooth_scroll::SmoothScrollIPC));
    m_ipc = nullptr;
  }

  if (m_fd != -1)
  {
    ::close(m_fd);
    m_fd = -1;
  }
}

bool IpcClient::connect()
{
  if (m_ipc)
  {
    return true;
  }

  m_fd = shm_open(smooth_scroll::IPC_SHM_NAME, O_RDWR, 0666);
  if (m_fd == -1)
  {
    return false;
  }

  void* addr = mmap(nullptr, sizeof(smooth_scroll::SmoothScrollIPC), PROT_READ | PROT_WRITE, MAP_SHARED, m_fd, 0);
  if (addr == MAP_FAILED)
  {
    ::close(m_fd);
    m_fd = -1;
    return false;
  }

  m_ipc = static_cast<smooth_scroll::SmoothScrollIPC*>(addr);
  if (!isMappedValid())
  {
    close();
    return false;
  }

  return true;
}

bool IpcClient::isMappedValid() const
{
  return m_ipc && m_ipc->magic_version.load(std::memory_order_relaxed) == smooth_scroll::IPC_MAGIC_VERSION_EXPECTED;
}

IpcSnapshot IpcClient::invalidSnapshot()
{
  return {};
}

}  // namespace SmoothScrollKWin
