// SPDX-License-Identifier: MIT
#include "profile.h"
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <cassert>
#include <fstream>
#include <sstream>
#include <spdlog/sinks/ostream_sink.h>
#include <spdlog/spdlog.h>
#include <unistd.h>

using namespace smooth_scroll;

int main(int argc, char** argv)
{
  assert(argc == 2);
  std::ostringstream warnings;
  auto logger = std::make_shared<spdlog::logger>("config-test", std::make_shared<spdlog::sinks::ostream_sink_mt>(warnings));
  logger->set_level(spdlog::level::warn);
  spdlog::set_default_logger(logger);
  Config shipped;
  assert(loadConfig(argv[1], shipped));
  assert(shipped.session.profiles.size() >= 2);
  const ProfileConfig defaults;
  const auto& global = shipped.session.profiles[0];
  assert(defaults.keyboard_braking_keys == global.keyboard_braking_keys);
  assert(defaults.keyboard_passthrough_keys == global.keyboard_passthrough_keys);
  assert(defaults.smoother.min_speed_change_upperbound == global.smoother.min_speed_change_upperbound);
  assert(defaults.smoother.max_speed_change_ratio == global.smoother.max_speed_change_ratio);
  const auto& compatibility = shipped.session.profiles[1];
  assert(compatibility.wheel_passthrough);
  assert(compatibility.free_spin_button == 0 && compatibility.drag_view_button == 0 &&
         compatibility.auto_scroll_button == 0);
  assert(compatibility.keyboard_braking_keys.empty() && compatibility.keyboard_passthrough_keys.empty());

  char path[] = "/tmp/smooth-scroll-profiles-XXXXXX";
  const int fd = mkstemp(path);
  assert(fd >= 0);
  close(fd);
  {
    std::ofstream file(path);
    file << "initial_speed = 999\nfree_spin_button = 0\n"
            "keyboard_passthrough_keys = [29]\n"
            "[[profiles]]\nname = 'Independent'\n"
            "keyboard_passthrough_keys = [42]\nmouse_movement_window_milliseconds = 40\n"
            "[[profiles]]\nwheel_passthrough = true\n"
            "free_spin_button = 0\ndrag_view_button = 0\nauto_scroll_button = 0\n"
            "keyboard_passthrough_keys = []\nkeyboard_braking_keys = []\n";
  }
  Config config;
  assert(loadConfig(path, config));
  {
    std::ofstream file(path);
    file << "[[profiles]]\n";
  }
  Config omitted;
  assert(loadConfig(path, omitted));
  for (const auto& profile : omitted.session.profiles)
  {
    assert(profile.keyboard_braking_keys == defaults.keyboard_braking_keys);
    assert(profile.keyboard_passthrough_keys == defaults.keyboard_passthrough_keys);
    assert(profile.smoother.min_speed_change_upperbound == defaults.smoother.min_speed_change_upperbound);
    assert(profile.smoother.max_speed_change_ratio == defaults.smoother.max_speed_change_ratio);
  }
  assert(warnings.str().empty()); // Missing options are an intentional use of built-in defaults.
  {
    std::ofstream file(path);
    file << "initial_speed = 'invalid'\nsmooth_mode = 99\ndevice_event_debounce_milliseconds = -1\n";
  }
  Config invalid;
  assert(loadConfig(path, invalid));
  assert(invalid.session.profiles[0].smoother.initial_speed == defaults.smoother.initial_speed);
  assert(invalid.session.profiles[0].smoother.smooth_mode == defaults.smoother.smooth_mode);
  assert(invalid.device_manager.debounce == DeviceConfig{}.debounce);
  const auto log = warnings.str();
  assert(log.find("Config 'initial_speed' invalid") != std::string::npos);
  assert(log.find("Config 'smooth_mode' invalid") != std::string::npos);
  assert(log.find("Config 'device_event_debounce_milliseconds' invalid") != std::string::npos);
  unlink(path);
  assert(config.session.profiles.size() == 3);
  const auto& base = config.session.profiles[0];
  const auto& alternate = config.session.profiles[1];
  assert(base.smoother.initial_speed == 999);
  assert(alternate.smoother.initial_speed == 600);
  assert(alternate.free_spin_button == BTN_RIGHT);
  assert(alternate.smoother.mouse_movement_window_milliseconds == 40);
  assert(config.session.profiles[2].wheel_passthrough);

  ProfileRuntime runtime{ alternate };
  KeyboardPassthroughState first, second;
  assert(first.update(KEY_LEFTCTRL, 1, false) == 1);
  assert(first.union_pressed == 1 && first.active_pressed == 0);
  assert(!runtime.canDeactivate(first.union_pressed));
  assert(second.update(KEY_LEFTCTRL, 1, false) == 1);
  assert(first.update(KEY_LEFTCTRL, 2, false) == 0);
  assert(first.update(KEY_LEFTCTRL, 1, false) == 0);
  assert(first.update(KEY_LEFTCTRL, 0, false) == -1);
  assert(!runtime.canDeactivate(first.union_pressed + second.union_pressed));
  assert(second.update(KEY_LEFTCTRL, 0, false) == -1);
  assert(runtime.canDeactivate(first.union_pressed + second.union_pressed));
  assert(first.update(KEY_LEFTSHIFT, 1, true) == 1);
  assert(first.active_pressed == 1);
  assert(first.update(KEY_LEFTSHIFT, 0, true) == -1);
  assert(first.active_pressed == 0);

  runtime.smoother.handleEvent({ 1, 0 }, true, false);
  assert(runtime.canDeactivate(0)); // Ordinary inertia may be stopped for switching.
  assert(runtime.smoother.handleFreeSpinButton(1));
  assert(!runtime.canDeactivate(0));
  assert(runtime.smoother.handleFreeSpinButton(0));
  runtime.smoother.stop();
  assert(!runtime.smoother.next_tick_time());
  runtime.smoother.handleEvent({ 2, 0 }, true, false);
  WheelSmoother fresh{ alternate.smoother };
  fresh.handleEvent({ 2, 0 }, true, false);
  assert(runtime.smoother.speed() == fresh.speed());

  auto special_config = alternate;
  special_config.smoother.auto_scroll_activation_mode = WheelSmoother::AutoScrollActivationMode::Always;
  special_config.smoother.auto_scroll_axis_mode = WheelSmoother::AutoScrollAxisMode::Omnidirectional;
  ProfileRuntime special{ special_config };
  special.smoother.handleAutoScrollButton({ 3, 0 }, BTN_MIDDLE, 1);
  assert(!special.canDeactivate(0));
  MouseFrameState frame{ 20, 0 };
  static_cast<void>(special.smoother.handleReportEvent({ 3, 1000 }, frame));
  special.smoother.handleAutoScrollButton({ 3, 2000 }, BTN_MIDDLE, 0);
  assert(!special.canDeactivate(0)); // Latched, even though no button is down.
  special.smoother.handleAutoScrollButton({ 3, 3000 }, BTN_MIDDLE, 1);
  assert(!special.canDeactivate(0)); // ExitHeld owns the release.
  special.smoother.handleAutoScrollButton({ 3, 4000 }, BTN_MIDDLE, 0);
  assert(special.canDeactivate(0));
}
