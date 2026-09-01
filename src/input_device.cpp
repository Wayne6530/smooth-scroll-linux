// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Wayne6530

#include "input_device.h"

#include <algorithm>
#include <cerrno>
#include <cstring>
#include <fcntl.h>
#include <unistd.h>
#include <utility>

#include <spdlog/spdlog.h>

namespace smooth_scroll
{
namespace
{

std::string valueOrEmpty(const char* value)
{
  return value ? value : "";
}

}  // namespace

bool DeviceCapabilities::isMouse() const noexcept
{
  return relative_axes[REL_X] && relative_axes[REL_Y] && relative_axes[REL_WHEEL];
}

bool DeviceCapabilities::supportsAnyKey(const std::vector<unsigned int>& keys_to_find) const noexcept
{
  for (unsigned int key : keys_to_find)
  {
    if (key < keys.size() && keys[key])
      return true;
  }
  return false;
}

bool DeviceCapabilities::contains(const DeviceCapabilities& other) const noexcept
{
  for (std::size_t i = 0; i < keys.size(); ++i)
  {
    if (other.keys[i] && !keys[i])
      return false;
  }
  for (std::size_t i = 0; i < relative_axes.size(); ++i)
  {
    if (other.relative_axes[i] && !relative_axes[i])
      return false;
  }
  for (std::size_t i = 0; i < miscellaneous.size(); ++i)
  {
    if (other.miscellaneous[i] && !miscellaneous[i])
      return false;
  }
  return true;
}

void DeviceCapabilities::merge(const DeviceCapabilities& other) noexcept
{
  for (std::size_t i = 0; i < keys.size(); ++i)
    keys[i] = keys[i] || other.keys[i];
  for (std::size_t i = 0; i < relative_axes.size(); ++i)
    relative_axes[i] = relative_axes[i] || other.relative_axes[i];
  for (std::size_t i = 0; i < miscellaneous.size(); ++i)
    miscellaneous[i] = miscellaneous[i] || other.miscellaneous[i];
}

bool DeviceInfo::isVirtualDevice() const noexcept
{
  return name == kVirtualDeviceName;
}

bool DeviceModel::matches(const DeviceInfo& device) const noexcept
{
  return vendor_id == device.vendor_id && product_id == device.product_id;
}

bool DeviceModel::operator==(const DeviceModel& other) const noexcept
{
  return vendor_id == other.vendor_id && product_id == other.product_id;
}

InputDeviceHandle::InputDeviceHandle(int fd, libevdev* evdev, DeviceInfo info) noexcept
  : fd_{ fd }, evdev_{ evdev }, info_{ std::move(info) }
{
}

InputDeviceHandle::~InputDeviceHandle()
{
  reset();
}

InputDeviceHandle::InputDeviceHandle(InputDeviceHandle&& other) noexcept
  : fd_{ std::exchange(other.fd_, -1) }, evdev_{ std::exchange(other.evdev_, nullptr) }, info_{ std::move(other.info_) }
{
}

InputDeviceHandle& InputDeviceHandle::operator=(InputDeviceHandle&& other) noexcept
{
  if (this != &other)
  {
    reset();
    fd_ = std::exchange(other.fd_, -1);
    evdev_ = std::exchange(other.evdev_, nullptr);
    info_ = std::move(other.info_);
  }
  return *this;
}

InputDeviceHandle::operator bool() const noexcept
{
  return fd_ >= 0 && evdev_;
}

int InputDeviceHandle::fd() const noexcept
{
  return fd_;
}

libevdev* InputDeviceHandle::evdev() const noexcept
{
  return evdev_;
}

const DeviceInfo& InputDeviceHandle::info() const noexcept
{
  return info_;
}

void InputDeviceHandle::reset() noexcept
{
  if (evdev_)
  {
    libevdev_free(evdev_);
    evdev_ = nullptr;
  }
  if (fd_ >= 0)
  {
    close(fd_);
    fd_ = -1;
  }
}

std::optional<InputDeviceHandle> openInputDevice(const std::string& path, bool log_errors)
{
  int fd = open(path.c_str(), O_RDONLY | O_NONBLOCK | O_CLOEXEC);
  if (fd < 0)
  {
    if (log_errors)
      SPDLOG_WARN("Failed to open device {}: {}", path, std::strerror(errno));
    return std::nullopt;
  }

  libevdev* evdev = nullptr;
  int result = libevdev_new_from_fd(fd, &evdev);
  if (result < 0)
  {
    if (log_errors)
      SPDLOG_WARN("Failed to initialize libevdev for {}: {}", path, std::strerror(-result));
    close(fd);
    return std::nullopt;
  }

  DeviceInfo info;
  info.path = path;
  info.name = valueOrEmpty(libevdev_get_name(evdev));
  info.bus_type = libevdev_get_id_bustype(evdev);
  info.vendor_id = libevdev_get_id_vendor(evdev);
  info.product_id = libevdev_get_id_product(evdev);

  if (libevdev_has_event_type(evdev, EV_KEY))
  {
    for (int code = 0; code <= KEY_MAX; ++code)
      info.capabilities.keys[code] = libevdev_has_event_code(evdev, EV_KEY, code);
  }
  if (libevdev_has_event_type(evdev, EV_REL))
  {
    for (int code = 0; code <= REL_MAX; ++code)
      info.capabilities.relative_axes[code] = libevdev_has_event_code(evdev, EV_REL, code);
  }
  if (libevdev_has_event_type(evdev, EV_MSC))
  {
    for (int code = 0; code <= MSC_MAX; ++code)
      info.capabilities.miscellaneous[code] = libevdev_has_event_code(evdev, EV_MSC, code);
  }

  return InputDeviceHandle{ fd, evdev, std::move(info) };
}

bool isIgnoredDevice(const DeviceInfo& device, const std::vector<DeviceModel>& ignored_devices) noexcept
{
  return std::any_of(ignored_devices.begin(), ignored_devices.end(),
                     [&](const DeviceModel& ignored) { return ignored.matches(device); });
}

}  // namespace smooth_scroll
