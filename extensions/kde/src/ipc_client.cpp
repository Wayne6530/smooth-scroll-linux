// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Wayne6530

#include "ipc_client.h"

#include <cerrno>
#include <csignal>
#include <fcntl.h>
#include <limits>
#include <poll.h>
#include <sys/stat.h>
#include <sys/syscall.h>
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
  if (!daemonAlive(pid))
  {
    return invalidSnapshot();
  }

  const uint32_t stateBits = m_ipc->state_bits.load(std::memory_order_relaxed);
  const uint32_t autoScrollOffset = m_ipc->auto_scroll_offset.load(std::memory_order_relaxed);

  IpcSnapshot snapshot;
  snapshot.valid = true;
  snapshot.pid = pid;
  snapshot.scrollId = m_ipc->scroll_id.load(std::memory_order_relaxed);
  snapshot.requestedProfileId =
      m_ipc->requested_profile_id.load(std::memory_order_relaxed);
  snapshot.activeProfileId = m_ipc->active_profile_id.load(std::memory_order_relaxed);
  snapshot.connected = (stateBits & smooth_scroll::IPC_STATE_CONNECTED) != 0;
  snapshot.keyboardPassthrough = (stateBits & smooth_scroll::IPC_STATE_KEYBOARD_PASSTHROUGH) != 0;
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

bool IpcClient::setRequestedProfile(uint32_t id)
{
  if (!connect() || !isMappedValid())
  {
    close();
    return false;
  }

  m_ipc->requested_profile_id.store(id, std::memory_order_relaxed);
  return true;
}

bool IpcClient::daemonAlive(uint32_t pid)
{
  const auto now = std::chrono::steady_clock::now();
  if (pid == m_pid && now < m_nextProbe)
    return m_alive;

  if (pid != m_pid && m_pidfd >= 0)
  {
    ::close(m_pidfd);
    m_pidfd = -1;
  }
  m_pid = pid;
  m_nextProbe = now + std::chrono::seconds(1);
  m_alive = false;
  if (!pid || pid > std::numeric_limits<pid_t>::max())
    return false;

  if (m_pidfd < 0)
    m_pidfd = syscall(SYS_pidfd_open, pid, 0);
  if (m_pidfd >= 0)
  {
    pollfd descriptor{m_pidfd, POLLIN, 0};
    m_alive = poll(&descriptor, 1, 0) == 0;
  }
  else
    m_alive = kill(static_cast<pid_t>(pid), 0) == 0 || errno == EPERM;
  return m_alive;
}

void IpcClient::close()
{
  if (m_pidfd >= 0)
    ::close(m_pidfd);
  m_pidfd = -1;
  m_pid = 0;
  m_alive = false;
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

  m_fd = shm_open(m_name.c_str(), O_RDWR, 0666);
  if (m_fd == -1)
  {
    return false;
  }

  struct stat info{};
  if (fstat(m_fd, &info) < 0 || info.st_size < static_cast<off_t>(sizeof(smooth_scroll::SmoothScrollIPC)))
  {
    close();
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
  return m_ipc && m_ipc->magic_version.load(std::memory_order_acquire) == smooth_scroll::IPC_MAGIC_VERSION_EXPECTED;
}

IpcSnapshot IpcClient::invalidSnapshot()
{
  return {};
}

}  // namespace SmoothScrollKWin
