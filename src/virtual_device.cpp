// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Wayne6530

#include "virtual_device.h"

#include <algorithm>
#include <cerrno>
#include <cstdio>
#include <cstring>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <unistd.h>

#include <linux/uinput.h>
#include <spdlog/spdlog.h>

namespace smooth_scroll
{

VirtualDevice::~VirtualDevice()
{
  reset();
}

bool VirtualDevice::ensureCapabilities(const DeviceCapabilities& required)
{
  DeviceCapabilities normalized = required;
  normalized.relative_axes[REL_WHEEL_HI_RES] = true;
  normalized.relative_axes[REL_HWHEEL_HI_RES] = true;

  if (fd_ >= 0 && capabilities_.contains(normalized))
    return true;

  DeviceCapabilities expanded = capabilities_;
  expanded.merge(normalized);
  destroy();
  return create(expanded);
}

bool VirtualDevice::create(const DeviceCapabilities& capabilities)
{
  fd_ = open("/dev/uinput", O_WRONLY | O_NONBLOCK | O_CLOEXEC);
  if (fd_ < 0)
  {
    SPDLOG_ERROR("Failed to open /dev/uinput: {}", std::strerror(errno));
    return false;
  }

  // Event types must be enabled before their individual codes.
  if (std::any_of(capabilities.keys.begin(), capabilities.keys.end(), [](bool value) { return value; }) &&
      ioctl(fd_, UI_SET_EVBIT, EV_KEY) < 0)
    goto create_failed;
  for (std::size_t code = 0; code < capabilities.keys.size(); ++code)
  {
    if (capabilities.keys[code] && ioctl(fd_, UI_SET_KEYBIT, static_cast<int>(code)) < 0)
      goto create_failed;
  }

  if (std::any_of(capabilities.relative_axes.begin(), capabilities.relative_axes.end(),
                  [](bool value) { return value; }) &&
      ioctl(fd_, UI_SET_EVBIT, EV_REL) < 0)
    goto create_failed;
  for (std::size_t code = 0; code < capabilities.relative_axes.size(); ++code)
  {
    if (capabilities.relative_axes[code] && ioctl(fd_, UI_SET_RELBIT, static_cast<int>(code)) < 0)
      goto create_failed;
  }

  if (std::any_of(capabilities.miscellaneous.begin(), capabilities.miscellaneous.end(),
                  [](bool value) { return value; }) &&
      ioctl(fd_, UI_SET_EVBIT, EV_MSC) < 0)
    goto create_failed;
  for (std::size_t code = 0; code < capabilities.miscellaneous.size(); ++code)
  {
    if (capabilities.miscellaneous[code] && ioctl(fd_, UI_SET_MSCBIT, static_cast<int>(code)) < 0)
      goto create_failed;
  }

  {
    uinput_user_dev descriptor{};
    std::snprintf(descriptor.name, UINPUT_MAX_NAME_SIZE, "%s", kVirtualDeviceName);
    descriptor.id.bustype = BUS_USB;
    descriptor.id.vendor = kVirtualDeviceVendor;
    descriptor.id.product = kVirtualDeviceProduct;
    descriptor.id.version = 1;

    if (write(fd_, &descriptor, sizeof(descriptor)) != static_cast<ssize_t>(sizeof(descriptor)) ||
        ioctl(fd_, UI_DEV_CREATE) < 0)
      goto create_failed;
  }

  capabilities_ = capabilities;
  SPDLOG_INFO("Created persistent uinput device");
  return true;

create_failed:
  SPDLOG_ERROR("Failed to create uinput device: {}", std::strerror(errno));
  close(fd_);
  fd_ = -1;
  return false;
}

void VirtualDevice::destroy() noexcept
{
  if (fd_ < 0)
    return;
  if (ioctl(fd_, UI_DEV_DESTROY) < 0)
    SPDLOG_WARN("Failed to destroy uinput device: {}", std::strerror(errno));
  close(fd_);
  fd_ = -1;
}

bool VirtualDevice::writeFrame(std::vector<input_event>& events, const timeval& time)
{
  if (events.empty())
    return true;

  events.push_back({ time, EV_SYN, SYN_REPORT, 0 });
  const ssize_t expected = static_cast<ssize_t>(events.size() * sizeof(input_event));
  const ssize_t written = write(fd_, events.data(), expected);
  events.clear();
  return written == expected;
}

void VirtualDevice::reset() noexcept
{
  destroy();
  capabilities_ = {};
}

}  // namespace smooth_scroll
