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
#include "profile.h"

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

  [[nodiscard]] static std::unique_ptr<Session> create(SessionResources resources, ProfileSet& profile_set,
                                                       VirtualDevice& virtual_device, IpcServer& ipc,
                                                       const std::atomic_bool& shutdown, int shutdown_fd,
                                                       CreateResult& result);
  ~Session();

  Session(const Session&) = delete;
  Session& operator=(const Session&) = delete;

  [[nodiscard]] RunResult run(const std::atomic_bool& shutdown);

private:
  friend struct SessionIpcTestAccess;

  struct KeyboardState
  {
    InputDeviceHandle device;
    KeyboardPassthroughState passthrough;
  };

  Session(SessionResources resources, ProfileSet& profile_set, VirtualDevice& virtual_device, IpcServer& ipc,
          int shutdown_fd);

  [[nodiscard]] CreateResult initialize(const std::atomic_bool& shutdown);
  [[nodiscard]] CreateResult discardPendingMouseEvents();
  [[nodiscard]] CreateResult waitUntilAllKeysReleased(const std::atomic_bool& shutdown);
  [[nodiscard]] bool anyPhysicalKeyPressed() const noexcept;
  [[nodiscard]] bool writeEvents(const timeval& time);
  void handleObservedKey(unsigned int code, int value, KeyboardPassthroughState& source);
  // Returns true when a brake or switch invalidates a previously scheduled tick.
  bool processIpcRequests();
  void publishProfileState();
  WheelSmoother& smoother() noexcept { return activeProfile().smoother; }
  void stopMotion();
  void updateAutoScrollIpc();

  InputDeviceHandle mouse_;
  std::vector<KeyboardState> keyboards_;
  std::unique_ptr<DeviceMonitor> monitor_;
  VirtualDevice& virtual_device_;
  IpcServer& ipc_;
  // Owned by the main loop; Session teardown resets the active smoother before reuse.
  ProfileSet& profile_set_;
  ProfileId active_profile_index_ = 0;
  ProfileRuntime& activeProfile() noexcept { return profile_set_.profiles[active_profile_index_]; }
  KeyboardPassthroughState mouse_passthrough_;
  std::vector<int> supported_mouse_keys_;
  std::vector<input_event> events_;
  MouseFrameState mouse_frame_;
  int num_union_pressed_ = 0;
  int num_passthrough_ = 0;
  int shutdown_fd_ = -1;
  bool grabbed_ = false;
};

}  // namespace smooth_scroll
