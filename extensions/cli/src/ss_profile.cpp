// SPDX-License-Identifier: MIT
#include <smooth_scroll/ipc_client.h>
#include <charconv>
#include <iostream>
#include <string_view>

int main(int argc, char* argv[])
{
  if (argc == 2 && (std::string_view{ argv[1] } == "--help" || std::string_view{ argv[1] } == "-h"))
  {
    std::cout << "Usage: ss-profile <unsigned-profile-id>\n"
                 "IDs follow configuration order: top-level = 0, [[profiles]] tables = 1, 2, ...\n"
                 "The default configuration uses profile 1 for compatibility passthrough.\n"
                 "Examples: ss-profile 1; ss-profile 0\n"
                 "Requests wait for special modes and all configured keyboard passthrough keys\n"
                 "to be released. Switching stops inertia. Idle requests may wait for input.\n"
                 "Unknown IDs leave the active profile unchanged. Use ss-status to compare\n"
                 "requested_profile_id with active_profile_id; success only confirms the write.\n"
                 "Requires a running IPC v3 daemon. Avoid competing writes from desktop plugins.\n";
    return 0;
  }
  uint32_t id = 0;
  if (argc != 2)
  {
    std::cerr << "Usage: ss-profile <unsigned-profile-id>\n";
    return 1;
  }
  const std::string_view argument{ argv[1] };
  const auto result = std::from_chars(argument.data(), argument.data() + argument.size(), id);
  if (result.ec != std::errc{} || result.ptr != argument.data() + argument.size())
    return 1;
  auto* ipc = smooth_scroll::connect_ipc();
  if (!ipc)
    return 1;
  ipc->requested_profile_id.store(id, std::memory_order_relaxed);
  munmap(ipc, sizeof(smooth_scroll::SmoothScrollIPC));
  return 0;
}
