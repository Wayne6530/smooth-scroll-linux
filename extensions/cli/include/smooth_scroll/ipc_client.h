// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Wayne6530

#pragma once

#include <smooth_scroll/ipc_protocol.h>

#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>

namespace smooth_scroll
{

inline SmoothScrollIPC* connect_ipc()
{
  int fd = shm_open(IPC_SHM_NAME, O_RDWR, 0666);
  if (fd == -1)
  {
    return nullptr;
  }

  void* addr = mmap(nullptr, sizeof(SmoothScrollIPC), PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
  close(fd);

  if (addr == MAP_FAILED)
  {
    return nullptr;
  }

  auto* ipc = static_cast<SmoothScrollIPC*>(addr);

  if (ipc->magic_version.load(std::memory_order_relaxed) != IPC_MAGIC_VERSION_EXPECTED)
  {
    munmap(ipc, sizeof(SmoothScrollIPC));
    return nullptr;
  }

  return ipc;
}

}  // namespace smooth_scroll
