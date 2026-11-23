// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Wayne6530

#pragma once

#include <smooth_scroll/ipc_protocol.h>

#include <chrono>
#include <cstdint>
#include <string>

namespace SmoothScrollKWin
{

struct IpcSnapshot
{
  bool valid = false;
  uint32_t pid = 0;
  uint32_t scrollId = 0;
  uint32_t requestedProfileId = 0;
  uint32_t activeProfileId = 0;
  bool connected = false;
  bool keyboardPassthrough = false;
  bool dragView = false;
  bool freeSpin = false;
  bool autoScroll = false;
  bool autoScrollHorizontalEnabled = false;
  bool autoScrollVerticalEnabled = false;
  int16_t autoScrollOffsetX = 0;
  int16_t autoScrollOffsetY = 0;
  bool horizontal = false;
  bool positive = false;
  uint32_t speed = 0;
};

class IpcClient
{
public:
  explicit IpcClient(const char* name = smooth_scroll::IPC_SHM_NAME) : m_name(name) {}
  ~IpcClient();

  IpcClient(const IpcClient&) = delete;
  IpcClient& operator=(const IpcClient&) = delete;

  IpcSnapshot readSnapshot();
  bool requestStop(const IpcSnapshot& snapshot);
  bool setRequestedProfile(uint32_t id);
  void close();

private:
  bool connect();
  bool isMappedValid() const;
  static IpcSnapshot invalidSnapshot();
  bool daemonAlive(uint32_t pid);

  std::string m_name;
  uint32_t m_pid = 0;
  int m_pidfd = -1;
  bool m_alive = false;
  std::chrono::steady_clock::time_point m_nextProbe{};

  int m_fd = -1;
  smooth_scroll::SmoothScrollIPC* m_ipc = nullptr;
};

}  // namespace SmoothScrollKWin
