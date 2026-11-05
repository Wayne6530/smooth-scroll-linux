// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Wayne6530

#include "config.h"

#ifdef NDEBUG
#undef NDEBUG
#endif
#include <cassert>
#include <cstdlib>
#include <fstream>
#include <string>

#include <unistd.h>

using namespace smooth_scroll;

int main(int argc, char* argv[])
{
  assert(argc == 4);

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
  assert(empty_config.session.profiles[0].smoother.auto_scroll_latch_on_release);
  assert(empty_config.session.profiles[0].smoother.auto_scroll_speed_exponent == 1.0);

  char temporary_path[] = "/tmp/smooth-scroll-config-XXXXXX";
  const int fd = mkstemp(temporary_path);
  assert(fd >= 0);
  close(fd);
  for (const char* exponent : { "0.5", "1.0", "2.0", "2.2", "3.0", "4.0", "1000.0" })
  {
    {
      std::ofstream file(temporary_path);
      file << "auto_scroll_latch_on_release = false\nauto_scroll_speed_exponent = " << exponent << "\n";
    }
    Config curve_config;
    const double value = std::stod(exponent);
    assert(loadConfig(temporary_path, curve_config));
    assert(!curve_config.session.profiles[0].smoother.auto_scroll_latch_on_release);
    assert(curve_config.session.profiles[0].smoother.auto_scroll_speed_exponent == value);
  }
  {
    std::ofstream file(temporary_path);
    file << "auto_scroll_latch_on_release = 123\nauto_scroll_speed_exponent = \"invalid\"\n";
  }
  Config fallback_config;
  assert(loadConfig(temporary_path, fallback_config));
  assert(fallback_config.session.profiles[0].smoother.auto_scroll_latch_on_release);
  assert(fallback_config.session.profiles[0].smoother.auto_scroll_speed_exponent == 1.0);
  unlink(temporary_path);

  Config invalid_config;
  assert(!loadConfig(argv[3], invalid_config));

  return 0;
}
