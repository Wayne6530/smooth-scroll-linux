// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Wayne6530

#pragma once

#include <array>
#include <optional>
#include <string>
#include <vector>

#include <libevdev-1.0/libevdev/libevdev.h>
#include <linux/input-event-codes.h>

namespace smooth_scroll
{

inline constexpr const char* kVirtualDeviceName = "Virtual Smooth Mouse";
inline constexpr unsigned int kVirtualDeviceVendor = 0x1234;
inline constexpr unsigned int kVirtualDeviceProduct = 0x5678;

struct DeviceCapabilities
{
  std::array<bool, KEY_MAX + 1> keys{};
  std::array<bool, REL_MAX + 1> relative_axes{};
  std::array<bool, MSC_MAX + 1> miscellaneous{};

  [[nodiscard]] bool isMouse() const noexcept;
  [[nodiscard]] bool supportsAnyKey(const std::vector<unsigned int>& keys_to_find) const noexcept;
  [[nodiscard]] bool contains(const DeviceCapabilities& other) const noexcept;
  void merge(const DeviceCapabilities& other) noexcept;
};

struct DeviceInfo
{
  std::string path;
  std::string name;
  unsigned int bus_type = 0;
  unsigned int vendor_id = 0;
  unsigned int product_id = 0;
  DeviceCapabilities capabilities;

  [[nodiscard]] bool isVirtualDevice() const noexcept;
};

struct DeviceModel
{
  unsigned int vendor_id = 0;
  unsigned int product_id = 0;

  [[nodiscard]] bool matches(const DeviceInfo& device) const noexcept;
  [[nodiscard]] bool operator==(const DeviceModel& other) const noexcept;
};

class InputDeviceHandle
{
public:
  InputDeviceHandle() = default;
  InputDeviceHandle(int fd, libevdev* evdev, DeviceInfo info) noexcept;
  ~InputDeviceHandle();

  InputDeviceHandle(const InputDeviceHandle&) = delete;
  InputDeviceHandle& operator=(const InputDeviceHandle&) = delete;

  InputDeviceHandle(InputDeviceHandle&& other) noexcept;
  InputDeviceHandle& operator=(InputDeviceHandle&& other) noexcept;

  [[nodiscard]] explicit operator bool() const noexcept;
  [[nodiscard]] int fd() const noexcept;
  [[nodiscard]] libevdev* evdev() const noexcept;
  [[nodiscard]] const DeviceInfo& info() const noexcept;

private:
  void reset() noexcept;

  int fd_{ -1 };
  libevdev* evdev_{ nullptr };
  DeviceInfo info_;
};

struct SessionDevices
{
  InputDeviceHandle mouse;
  std::vector<InputDeviceHandle> keyboards;
};

[[nodiscard]] std::optional<InputDeviceHandle> openInputDevice(const std::string& path, bool log_errors = true);
[[nodiscard]] bool isIgnoredDevice(const DeviceInfo& device, const std::vector<DeviceModel>& ignored_devices) noexcept;
[[nodiscard]] constexpr bool isPointerButton(unsigned int code) noexcept
{
  return code >= BTN_MOUSE && code <= BTN_TASK;
}

}  // namespace smooth_scroll
