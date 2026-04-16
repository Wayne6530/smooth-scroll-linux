// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Wayne6530

#include "ipc_client.h"

#include <string>

int main(int argc, char* argv[])
{
  auto* ipc = smooth_scroll::connect_ipc();
  if (!ipc)
    return 1;

  uint32_t current_val = ipc->force_passthrough.load(std::memory_order_relaxed);
  uint32_t new_val = 0;

  if (argc > 1)
  {
    std::string arg = argv[1];
    if (arg == "1" || arg == "on" || arg == "true")
    {
      new_val = 1;
    }
    else if (arg == "0" || arg == "off" || arg == "false")
    {
      new_val = 0;
    }
    else
    {
      munmap(ipc, sizeof(smooth_scroll::SmoothScrollIPC));
      return 1;
    }
  }
  else
  {
    new_val = (current_val > 0) ? 0 : 1;
  }

  ipc->force_passthrough.store(new_val, std::memory_order_relaxed);

  munmap(ipc, sizeof(smooth_scroll::SmoothScrollIPC));
  return 0;
}
