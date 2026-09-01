// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Wayne6530

#include "config.h"

#include <algorithm>
#include <cerrno>
#include <cstdint>
#include <cstring>
#include <limits>

#include <fmt/ranges.h>
#include <spdlog/spdlog.h>
#include <toml++/toml.hpp>
#include <unistd.h>

namespace smooth_scroll
{
namespace
{

template <typename T>
void readOption(const toml::table& table, const char* name, T& value)
{
  if (auto option = table[name].value<T>())
  {
    value = *option;
    SPDLOG_INFO("Config loaded: {} = {}", name, value);
  }
  else
  {
    SPDLOG_WARN("Config '{}' not found or invalid, using default: {}", name, value);
  }
}

template <typename Enum>
void readEnum(const toml::table& table, const char* name, Enum& value, int minimum, int maximum)
{
  if (auto option = table[name].value<int>(); option && *option >= minimum && *option <= maximum)
  {
    value = static_cast<Enum>(*option);
    SPDLOG_INFO("Config loaded: {} = {}", name, *option);
  }
  else
  {
    SPDLOG_WARN("Config '{}' not found or invalid, using default: {}", name, static_cast<int>(value));
  }
}

bool readKeyList(const toml::table& table, const char* name, std::vector<unsigned int>& keys)
{
  keys.clear();
  if (const auto* array = table[name].as_array())
  {
    for (const auto& element : *array)
    {
      if (auto value = element.value<unsigned int>(); value && *value < KEY_CNT)
      {
        if (isPointerButton(*value))
        {
          SPDLOG_ERROR("Config '{}' cannot contain pointer button code {} (BTN_MOUSE through BTN_TASK)", name, *value);
          return false;
        }
        keys.push_back(*value);
      }
    }
  }
  std::sort(keys.begin(), keys.end());
  keys.erase(std::unique(keys.begin(), keys.end()), keys.end());
  SPDLOG_INFO("Use {} {}", name, keys);
  return true;
}

bool readIgnoredDevices(const toml::table& table, std::vector<DeviceModel>& ignored_devices)
{
  const auto node = table["ignored_devices"];
  if (!node)
    return true;

  const auto* array = node.as_array();
  if (!array)
  {
    SPDLOG_ERROR("Config 'ignored_devices' must be an array of device model tables");
    return false;
  }

  std::size_t index = 0;
  for (const auto& element : *array)
  {
    const auto* device_table = element.as_table();
    if (!device_table)
    {
      SPDLOG_ERROR("Config 'ignored_devices[{}]' must be a table", index);
      return false;
    }

    const auto vendor_id = (*device_table)["vendor_id"].value<unsigned int>();
    const auto product_id = (*device_table)["product_id"].value<unsigned int>();
    constexpr unsigned int maximum_id = std::numeric_limits<std::uint16_t>::max();
    if (!vendor_id || !product_id || *vendor_id > maximum_id || *product_id > maximum_id)
    {
      SPDLOG_ERROR("Config 'ignored_devices[{}]' must contain 16-bit unsigned vendor_id and product_id values", index);
      return false;
    }

    for (const auto& [key, value] : *device_table)
    {
      static_cast<void>(value);
      if (key != "vendor_id" && key != "product_id")
      {
        SPDLOG_ERROR("Config 'ignored_devices[{}]' contains unsupported field '{}'", index, key.str());
        return false;
      }
    }

    DeviceModel model{ *vendor_id, *product_id };
    if (std::find(ignored_devices.begin(), ignored_devices.end(), model) == ignored_devices.end())
      ignored_devices.push_back(model);
    ++index;
  }

  SPDLOG_INFO("Config loaded: ignored_devices contains {} device model(s)", ignored_devices.size());
  return true;
}

}  // namespace

bool loadConfig(const std::string& path, Config& config)
{
  if (access(path.c_str(), R_OK) != 0)
    SPDLOG_INFO("Config file '{}' is not readable: {}", path, std::strerror(errno));

  toml::table table;
  try
  {
    table = toml::parse_file(path);
  }
  catch (const toml::parse_error& error)
  {
    SPDLOG_WARN("Parsing failed: {}", error.description());
  }

  if (auto value = table["device_event_debounce_milliseconds"].value<int>(); value && *value >= 0)
  {
    config.device_manager.debounce = std::chrono::milliseconds{ *value };
    SPDLOG_INFO("Config loaded: device_event_debounce_milliseconds = {}", *value);
  }
  else
  {
    SPDLOG_WARN("Config 'device_event_debounce_milliseconds' not found or invalid, using default: {}",
                config.device_manager.debounce.count());
  }

  if (!readIgnoredDevices(table, config.device_manager.ignored_devices))
    return false;

  readOption(table, "free_spin_button", config.session.free_spin_button);
  readOption(table, "drag_view_button", config.session.drag_view_button);
  readOption(table, "auto_scroll_button", config.session.auto_scroll_button);

  const auto buttonsConflict = [](int lhs, int rhs) { return lhs != 0 && lhs == rhs; };
  if (buttonsConflict(config.session.auto_scroll_button, config.session.drag_view_button) ||
      buttonsConflict(config.session.auto_scroll_button, config.session.free_spin_button))
  {
    SPDLOG_ERROR(
        "Conflicting mode button configuration: auto_scroll_button={}, drag_view_button={}, free_spin_button={}. "
        "An enabled Auto Scroll button must differ from the Drag View and Free Spin buttons.",
        config.session.auto_scroll_button, config.session.drag_view_button, config.session.free_spin_button);
    return false;
  }

  if (!readKeyList(table, "keyboard_braking_keys", config.session.keyboard_braking_keys) ||
      !readKeyList(table, "keyboard_passthrough_keys", config.session.keyboard_passthrough_keys))
    return false;

  auto& options = config.session.smoother;
  readEnum(table, "smooth_mode", options.smooth_mode, 0, 2);
  readOption(table, "wheel_tick_distance", options.wheel_tick_distance);
  readOption(table, "tick_interval_microseconds", options.tick_interval_microseconds);
  readOption(table, "min_deceleration", options.min_deceleration);
  readOption(table, "max_deceleration", options.max_deceleration);
  readOption(table, "initial_speed", options.initial_speed);
  readOption(table, "speed_factor", options.speed_factor);
  readOption(table, "speed_smooth_window_microseconds", options.speed_smooth_window_microseconds);
  readOption(table, "max_speed_change_lowerbound", options.max_speed_change_lowerbound);
  readOption(table, "min_speed_change_upperbound", options.min_speed_change_upperbound);
  readOption(table, "min_speed_change_ratio", options.min_speed_change_ratio);
  readOption(table, "max_speed_change_ratio", options.max_speed_change_ratio);
  readOption(table, "damping", options.damping);
  readOption(table, "use_reverse_scroll_braking", options.use_reverse_scroll_braking);
  readOption(table, "max_reverse_scroll_braking_microseconds", options.max_reverse_scroll_braking_microseconds);
  readOption(table, "max_reverse_scroll_braking_times", options.max_reverse_scroll_braking_times);
  readOption(table, "reverse_scroll_intent_window_microseconds", options.reverse_scroll_intent_window_microseconds);
  readOption(table, "use_mouse_movement_braking", options.use_mouse_movement_braking);
  readOption(table, "max_mouse_movement_distance", options.max_mouse_movement_distance);
  readOption(table, "mouse_movement_window_milliseconds", options.mouse_movement_window_milliseconds);
  readOption(table, "mouse_movement_delay_microseconds", options.mouse_movement_delay_microseconds);
  readEnum(table, "drag_view_activation_mode", options.drag_view_activation_mode, 0, 1);
  readOption(table, "drag_view_click_timeout_milliseconds", options.drag_view_click_timeout_milliseconds);
  readOption(table, "drag_view_speed", options.drag_view_speed);
  readEnum(table, "auto_scroll_activation_mode", options.auto_scroll_activation_mode, 0, 1);
  readEnum(table, "auto_scroll_axis_mode", options.auto_scroll_axis_mode, 0, 2);
  readEnum(table, "auto_scroll_wheel_action", options.auto_scroll_wheel_action, 0, 1);
  readEnum(table, "auto_scroll_exit_button_mode", options.auto_scroll_exit_button_mode, 0, 1);
  readOption(table, "auto_scroll_deadzone", options.auto_scroll_deadzone);
  readOption(table, "auto_scroll_click_timeout_milliseconds", options.auto_scroll_click_timeout_milliseconds);
  readOption(table, "auto_scroll_speed_factor", options.auto_scroll_speed_factor);
  readOption(table, "auto_scroll_max_speed", options.auto_scroll_max_speed);
  return true;
}

}  // namespace smooth_scroll
