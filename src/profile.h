// SPDX-License-Identifier: MIT
#pragma once

#include <array>
#include <cassert>
#include <vector>
#include "config.h"

namespace smooth_scroll
{
struct ProfileRuntime
{
  explicit ProfileRuntime(const ProfileConfig& value) : config(value), smoother(value.smoother)
  {
    for (auto key : config.keyboard_braking_keys)
    {
      assert(key < KEY_CNT && !isPointerButton(key));
      braking_keys[key] = true;
    }
    for (auto key : config.keyboard_passthrough_keys)
    {
      assert(key < KEY_CNT && !isPointerButton(key));
      passthrough_keys[key] = true;
      braking_keys[key] = true;
    }
  }
  bool canDeactivate(int union_pressed) const noexcept
  {
    return union_pressed == 0 && !smoother.specialModeActive();
  }

  const ProfileConfig& config;
  WheelSmoother smoother;
  std::array<bool, KEY_CNT> braking_keys{};
  std::array<bool, KEY_CNT> passthrough_keys{};
};

// Configuration-derived data and processors shared across successive Sessions.
struct ProfileSet
{
  explicit ProfileSet(const SessionConfig& config)
  {
    assert(!config.profiles.empty());
    profiles.reserve(config.profiles.size());
    for (const auto& profile : config.profiles)
    {
      profiles.emplace_back(profile);
      for (auto key : profile.keyboard_passthrough_keys)
        passthrough_union_keys[key] = true;
    }
  }

  std::vector<ProfileRuntime> profiles;
  std::array<bool, KEY_CNT> passthrough_union_keys{};
};

// Physical state belongs to an input source, not a profile.
struct KeyboardPassthroughState
{
  std::array<bool, KEY_CNT> pressed{};
  int union_pressed = 0;
  int active_pressed = 0;

  int update(unsigned int code, int value, bool active) noexcept
  {
    if (value == 2 || pressed[code] == (value != 0))
      return 0;
    pressed[code] = value != 0;
    const int change = value ? 1 : -1;
    union_pressed += change;
    if (active)
      active_pressed += change;
    return change;
  }
};
}  // namespace smooth_scroll
