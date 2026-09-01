// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Wayne6530

#pragma once

#include <array>
#include <atomic>
#include <memory>
#include <vector>

#include "device_manager.h"
#include "ipc_server.h"
#include "virtual_device.h"
#include "wheel_smoother.h"

namespace smooth_scroll
{

class Session
{
public:
  enum class RunResult
  {
    Shutdown,
    RestartRequested,
    MouseLost,
    InputOutputError,
  };

  enum class CreateResult
  {
    Success,
    Shutdown,
    MouseLost,
    InputOutputError,
  };

  [[nodiscard]] static std::unique_ptr<Session> create(SessionResources resources, const SessionConfig& config,
                                                       VirtualDevice& virtual_device, IpcServer& ipc,
                                                       const std::atomic_bool& shutdown, int shutdown_fd,
                                                       CreateResult& result);
  ~Session();

  Session(const Session&) = delete;
  Session& operator=(const Session&) = delete;

  [[nodiscard]] RunResult run(const std::atomic_bool& shutdown);

private:
  struct KeyboardState
  {
    InputDeviceHandle device;
    std::array<bool, KEY_CNT> passthrough_pressed{};
    int num_passthrough = 0;
  };

  Session(SessionResources resources, const SessionConfig& config, VirtualDevice& virtual_device, IpcServer& ipc,
          int shutdown_fd);

  [[nodiscard]] CreateResult initialize(const std::atomic_bool& shutdown);
  [[nodiscard]] CreateResult discardPendingMouseEvents();
  [[nodiscard]] CreateResult waitUntilAllKeysReleased(const std::atomic_bool& shutdown);
  [[nodiscard]] bool anyPhysicalKeyPressed() const noexcept;
  [[nodiscard]] bool writeEvents(const timeval& time);
  void handleObservedKey(unsigned int code, int value, std::array<bool, KEY_CNT>& passthrough_pressed,
                         int& source_num_passthrough);
  void stopMotion();
  void updateAutoScrollIpc();

  InputDeviceHandle mouse_;
  std::vector<KeyboardState> keyboards_;
  std::unique_ptr<DeviceMonitor> monitor_;
  const SessionConfig& config_;
  VirtualDevice& virtual_device_;
  IpcServer& ipc_;
  WheelSmoother wheel_smoother_;
  std::array<bool, KEY_CNT> braking_keys_{};
  std::array<bool, KEY_CNT> passthrough_keys_{};
  std::array<bool, KEY_CNT> mouse_passthrough_pressed_{};
  std::vector<int> supported_mouse_keys_;
  std::vector<input_event> events_;
  int mouse_num_passthrough_ = 0;
  int num_passthrough_ = 0;
  int shutdown_fd_ = -1;
  bool grabbed_ = false;
};

}  // namespace smooth_scroll
