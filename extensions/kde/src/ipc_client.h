// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Wayne6530

#pragma once

#include <smooth_scroll/ipc_protocol.h>

#include <cstdint>

namespace SmoothScrollKWin
{

struct IpcSnapshot
{
  bool valid = false;
  uint32_t pid = 0;
  uint32_t scrollId = 0;
  uint32_t forcePassthrough = 0;
  bool connected = false;
  bool passthrough = false;
  bool dragView = false;
  bool freeSpin = false;
  bool horizontal = false;
  bool positive = false;
  uint32_t speed = 0;
};

class IpcClient
{
public:
  IpcClient() = default;
  ~IpcClient();

  IpcClient(const IpcClient&) = delete;
  IpcClient& operator=(const IpcClient&) = delete;

  IpcSnapshot readSnapshot();
  bool requestStop(const IpcSnapshot& snapshot);
  bool setForcePassthrough(bool enabled);
  void close();

private:
  bool connect();
  bool isMappedValid() const;
  static IpcSnapshot invalidSnapshot();

  int m_fd = -1;
  smooth_scroll::SmoothScrollIPC* m_ipc = nullptr;
};

}  // namespace SmoothScrollKWin
