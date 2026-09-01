// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Wayne6530

#include "config.h"

#ifdef NDEBUG
#undef NDEBUG
#endif
#include <cassert>
#include <string>

using namespace smooth_scroll;

int main(int argc, char* argv[])
{
  assert(argc == 6);

  DeviceCapabilities base_capabilities;
  base_capabilities.relative_axes[REL_X] = true;
  base_capabilities.relative_axes[REL_Y] = true;
  DeviceCapabilities wheel_capabilities;
  wheel_capabilities.relative_axes[REL_WHEEL] = true;
  assert(!base_capabilities.contains(wheel_capabilities));
  base_capabilities.merge(wheel_capabilities);
  assert(base_capabilities.contains(wheel_capabilities));
  assert(base_capabilities.isMouse());
  assert(isPointerButton(BTN_LEFT));
  assert(isPointerButton(BTN_TASK));
  assert(!isPointerButton(KEY_LEFTCTRL));
  assert(!isPointerButton(KEY_OK));

  Config config;
  assert(loadConfig(argv[1], config));
  assert(config.device_manager.debounce.count() == 750);
  assert(config.device_manager.ignored_devices.size() == 2);

  DeviceInfo device;
  device.vendor_id = 0x046d;
  device.product_id = 0xc547;
  assert(isIgnoredDevice(device, config.device_manager.ignored_devices));

  device.vendor_id = 0x1234;
  assert(!isIgnoredDevice(device, config.device_manager.ignored_devices));
  assert(!isIgnoredDevice(device, {}));

  device.vendor_id = 0x5678;
  device.product_id = 0xef01;
  assert(isIgnoredDevice(device, config.device_manager.ignored_devices));

  Config empty_config;
  assert(loadConfig(argv[2], empty_config));
  assert(empty_config.device_manager.ignored_devices.empty());

  Config invalid_config;
  assert(!loadConfig(argv[3], invalid_config));

  Config invalid_pointer_braking_config;
  assert(!loadConfig(argv[4], invalid_pointer_braking_config));

  Config invalid_pointer_passthrough_config;
  assert(!loadConfig(argv[5], invalid_pointer_passthrough_config));
  return 0;
}
