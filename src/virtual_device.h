// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Wayne6530

#pragma once

#include <vector>

#include <linux/input.h>

#include "input_device.h"

namespace smooth_scroll
{

class VirtualDevice
{
public:
  VirtualDevice() = default;
  ~VirtualDevice();

  VirtualDevice(const VirtualDevice&) = delete;
  VirtualDevice& operator=(const VirtualDevice&) = delete;

  // Precondition: no virtual key is pressed.
  [[nodiscard]] bool ensureCapabilities(const DeviceCapabilities& required);
  [[nodiscard]] bool writeFrame(std::vector<input_event>& events, const timeval& time);
  void reset() noexcept;

private:
  [[nodiscard]] bool create(const DeviceCapabilities& capabilities);
  void destroy() noexcept;

  int fd_{ -1 };
  DeviceCapabilities capabilities_;
};

}  // namespace smooth_scroll
