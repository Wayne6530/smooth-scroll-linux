// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Wayne6530

#pragma once

#include <cstdint>
#include <atomic>
#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>

namespace smooth_scroll
{

struct alignas(32) SmoothScrollIPC
{
  std::atomic<uint32_t> magic_version;
  std::atomic<uint32_t> daemon_pid;
  std::atomic<uint32_t> state_bits;
  std::atomic<uint32_t> scroll_id;
  std::atomic<uint32_t> force_passthrough;
  std::atomic<uint32_t> reserved[3];
};

static constexpr uint32_t MAGIC_VERSION_EXPECTED = 0x53530001;
static constexpr const char* SHM_NAME = "/smooth_scroll_shm";

inline SmoothScrollIPC* connect_ipc()
{
  int fd = shm_open(SHM_NAME, O_RDWR, 0666);
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

  if (ipc->magic_version.load(std::memory_order_relaxed) != MAGIC_VERSION_EXPECTED)
  {
    munmap(ipc, sizeof(SmoothScrollIPC));
    return nullptr;
  }

  return ipc;
}

}  // namespace smooth_scroll
