// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Wayne6530

#include "ipc_client.h"

#include <thread>
#include <chrono>
#include <iostream>

int main()
{
  auto* ipc = smooth_scroll::connect_ipc();
  if (!ipc)
    return 1;

  uint32_t last_state = 0xFFFFFFFF;

  while (true)
  {
    uint32_t pid = ipc->daemon_pid.load(std::memory_order_relaxed);
    if (pid == 0)
    {
      break;
    }

    uint32_t current_state = ipc->state_bits.load(std::memory_order_relaxed);

    if (current_state != last_state)
    {
      bool connected = current_state & (1 << 0);
      bool passthrough = current_state & (1 << 1);
      bool drag_view = current_state & (1 << 2);
      bool free_spin = current_state & (1 << 3);
      bool horizontal = current_state & (1 << 4);
      bool direction = current_state & (1 << 5);
      uint16_t speed = current_state >> 16;

      std::cout << "{"
                << "\"pid\":" << pid << ","
                << "\"connected\":" << (connected ? "true" : "false") << ","
                << "\"passthrough\":" << (passthrough ? "true" : "false") << ","
                << "\"drag_view\":" << (drag_view ? "true" : "false") << ","
                << "\"free_spin\":" << (free_spin ? "true" : "false") << ","
                << "\"horizontal\":" << (horizontal ? "true" : "false") << ","
                << "\"direction\":\"" << (direction ? "positive" : "negative") << "\","
                << "\"speed\":" << speed << "}\n"
                << std::flush;

      last_state = current_state;
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(16));
  }

  munmap(ipc, sizeof(smooth_scroll::SmoothScrollIPC));
  return 0;
}
