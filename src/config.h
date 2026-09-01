// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Wayne6530

#pragma once

#include <chrono>
#include <string>
#include <vector>

#include "input_device.h"
#include "wheel_smoother.h"

namespace smooth_scroll
{

struct DeviceConfig
{
  std::chrono::milliseconds debounce{ 500 };
  std::vector<DeviceModel> ignored_devices;
};

struct SessionConfig
{
  int free_spin_button = BTN_RIGHT;
  int drag_view_button = BTN_MIDDLE;
  int auto_scroll_button = 0;
  std::vector<unsigned int> keyboard_braking_keys;
  std::vector<unsigned int> keyboard_passthrough_keys;
  WheelSmoother::Options smoother;
};

struct Config
{
  DeviceConfig device_manager;
  SessionConfig session;
};

[[nodiscard]] bool loadConfig(const std::string& path, Config& config);

}  // namespace smooth_scroll
