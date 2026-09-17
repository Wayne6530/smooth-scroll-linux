// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Wayne6530

#include "ipc_server.h"

#include <cassert>
#include <cstdlib>
#include <cstdint>
#include <iostream>
#include <string>

#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>

namespace
{

using smooth_scroll::IPC_STATE_KEYBOARD_PASSTHROUGH;
using smooth_scroll::IpcServer;
using smooth_scroll::SmoothScrollIPC;

void testCompatibilityRequestDoesNotBrake()
{
  const std::string shm_name = "/smooth_scroll_ipc_test_" + std::to_string(getpid());
  shm_unlink(shm_name.c_str());

  {
    IpcServer server{ shm_name };
    const bool initialized = server.initialize();
    assert(initialized);
    if (!initialized)
      std::abort();

    const int fd = shm_open(shm_name.c_str(), O_RDWR, 0);
    assert(fd >= 0);
    if (fd < 0)
      std::abort();
    void* address = mmap(nullptr, sizeof(SmoothScrollIPC), PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    assert(address != MAP_FAILED);
    if (address == MAP_FAILED)
      std::abort();
    auto* ipc = static_cast<SmoothScrollIPC*>(address);

    assert(!server.isCompatibilityPassthroughRequested());
    assert(!server.checkBrakeRequest());

    ipc->compatibility_passthrough_requested.store(1, std::memory_order_relaxed);
    assert(server.isCompatibilityPassthroughRequested());
    assert(!server.checkBrakeRequest());

    server.setKeyboardPassthrough(true);
    assert((ipc->state_bits.load(std::memory_order_relaxed) & IPC_STATE_KEYBOARD_PASSTHROUGH) != 0);
    server.setKeyboardPassthrough(false);
    assert((ipc->state_bits.load(std::memory_order_relaxed) & IPC_STATE_KEYBOARD_PASSTHROUGH) == 0);

    ipc->scroll_id.fetch_add(1, std::memory_order_relaxed);
    assert(server.checkBrakeRequest());
    assert(!server.checkBrakeRequest());

    if (munmap(address, sizeof(SmoothScrollIPC)) != 0)
      std::abort();
    if (close(fd) != 0)
      std::abort();
  }

  if (shm_unlink(shm_name.c_str()) != 0)
    std::abort();
}

}  // namespace

int main()
{
  testCompatibilityRequestDoesNotBrake();
  std::cout << "ipc_server_test: all tests passed\n";
  return 0;
}
