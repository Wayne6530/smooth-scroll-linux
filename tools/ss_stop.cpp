// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Wayne6530

#include "ipc_client.h"

#include <iostream>

int main()
{
  auto* ipc = smooth_scroll::connect_ipc();
  if (!ipc)
    return 1;

  ipc->scroll_id.fetch_add(1, std::memory_order_relaxed);

  munmap(ipc, sizeof(smooth_scroll::SmoothScrollIPC));
  return 0;
}
