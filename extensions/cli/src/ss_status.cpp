// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Wayne6530

#include <smooth_scroll/ipc_client.h>

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
      bool connected = current_state & smooth_scroll::IPC_STATE_CONNECTED;
      bool passthrough = current_state & smooth_scroll::IPC_STATE_PASSTHROUGH;
      bool drag_view = current_state & smooth_scroll::IPC_STATE_DRAG_VIEW;
      bool free_spin = current_state & smooth_scroll::IPC_STATE_FREE_SPIN;
      bool horizontal = current_state & smooth_scroll::IPC_STATE_HORIZONTAL;
      bool direction = current_state & smooth_scroll::IPC_STATE_DIRECTION;
      uint16_t speed = current_state >> smooth_scroll::IPC_STATE_SPEED_SHIFT;

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
