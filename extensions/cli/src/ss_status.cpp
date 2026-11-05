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
  uint32_t last_auto_scroll_offset = 0xFFFFFFFF;
  uint32_t last_requested_profile_id = 0xFFFFFFFF;
  uint32_t last_active_profile_id = 0xFFFFFFFF;

  while (true)
  {
    uint32_t pid = ipc->daemon_pid.load(std::memory_order_relaxed);
    if (pid == 0)
    {
      break;
    }

    uint32_t current_state = ipc->state_bits.load(std::memory_order_relaxed);
    uint32_t current_auto_scroll_offset = ipc->auto_scroll_offset.load(std::memory_order_relaxed);
    uint32_t current_requested_profile_id =
        ipc->requested_profile_id.load(std::memory_order_relaxed);

    const uint32_t active_profile_id = ipc->active_profile_id.load(std::memory_order_relaxed);
    if (active_profile_id != last_active_profile_id || current_state != last_state || current_auto_scroll_offset != last_auto_scroll_offset ||
        current_requested_profile_id != last_requested_profile_id)
    {
      bool connected = current_state & smooth_scroll::IPC_STATE_CONNECTED;
      bool keyboard_passthrough = current_state & smooth_scroll::IPC_STATE_KEYBOARD_PASSTHROUGH;

      bool drag_view = current_state & smooth_scroll::IPC_STATE_DRAG_VIEW;
      bool free_spin = current_state & smooth_scroll::IPC_STATE_FREE_SPIN;
      bool horizontal = current_state & smooth_scroll::IPC_STATE_HORIZONTAL;
      bool direction = current_state & smooth_scroll::IPC_STATE_DIRECTION;
      bool auto_scroll = current_state & smooth_scroll::IPC_STATE_AUTO_SCROLL;
      bool auto_scroll_horizontal_enabled =
          current_state & smooth_scroll::IPC_STATE_AUTO_SCROLL_HORIZONTAL_ENABLED;
      bool auto_scroll_vertical_enabled = current_state & smooth_scroll::IPC_STATE_AUTO_SCROLL_VERTICAL_ENABLED;
      int16_t auto_scroll_offset_x = smooth_scroll::autoScrollHorizontalOffset(current_auto_scroll_offset);
      int16_t auto_scroll_offset_y = smooth_scroll::autoScrollVerticalOffset(current_auto_scroll_offset);
      uint16_t speed = current_state >> smooth_scroll::IPC_STATE_SPEED_SHIFT;

      std::cout << "{"
                << "\"pid\":" << pid << ","
                << "\"connected\":" << (connected ? "true" : "false") << ","
                << "\"keyboard_passthrough\":" << (keyboard_passthrough ? "true" : "false") << ","
                << "\"requested_profile_id\":"
                << current_requested_profile_id << ","
                << "\"active_profile_id\":" << active_profile_id << ","
                << "\"drag_view\":" << (drag_view ? "true" : "false") << ","
                << "\"free_spin\":" << (free_spin ? "true" : "false") << ","
                << "\"auto_scroll\":" << (auto_scroll ? "true" : "false") << ","
                << "\"auto_scroll_horizontal_enabled\":"
                << (auto_scroll_horizontal_enabled ? "true" : "false") << ","
                << "\"auto_scroll_vertical_enabled\":" << (auto_scroll_vertical_enabled ? "true" : "false")
                << ","
                << "\"auto_scroll_offset_x\":" << auto_scroll_offset_x << ","
                << "\"auto_scroll_offset_y\":" << auto_scroll_offset_y << ","
                << "\"horizontal\":" << (horizontal ? "true" : "false") << ","
                << "\"direction\":\"" << (direction ? "positive" : "negative") << "\","
                << "\"speed\":" << speed << "}\n"
                << std::flush;

      last_active_profile_id = active_profile_id;
      last_state = current_state;
      last_auto_scroll_offset = current_auto_scroll_offset;
      last_requested_profile_id = current_requested_profile_id;
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(16));
  }

  munmap(ipc, sizeof(smooth_scroll::SmoothScrollIPC));
  return 0;
}
