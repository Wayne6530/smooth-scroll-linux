// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Wayne6530

#pragma once

#include <chrono>
#include <optional>
#include <vector>

#include <linux/input.h>

namespace smooth_scroll
{

class WheelSmoother
{
public:
  struct Options
  {
    int tick_interval_microseconds = 2000;

    double min_speed = 0;
    double min_deceleration = 1420;
    double max_deceleration = 6000;
    double initial_speed = 600;
    double speed_factor = 40;
    int speed_smooth_window_microseconds = 200000;
    double max_speed_increase_per_wheel_event = 1200;
    double max_speed_decrease_per_wheel_event = 0;
    double damping = 3.1;

    bool use_braking = true;
    int braking_dejitter_microseconds = 100000;
    int max_braking_times = 3;
    double braking_cut_off_speed = 1000;
    double speed_decrease_per_braking = std::numeric_limits<double>::infinity();

    bool use_mouse_movement_braking = true;
    int mouse_movement_dejitter_distance = 200;
    int max_mouse_movement_event_interval_microseconds = 50000;
    double mouse_movement_braking_cut_off_speed = 200;
    double speed_decrease_per_mouse_movement = std::numeric_limits<double>::infinity();
  };

  explicit WheelSmoother(const Options& options);

  WheelSmoother(const WheelSmoother&) = delete;
  WheelSmoother& operator=(const WheelSmoother&) = delete;

  WheelSmoother(WheelSmoother&&) = delete;
  WheelSmoother& operator=(WheelSmoother&&) = delete;

  void stop();

  bool setFreeSpin(bool enabled);

  std::optional<struct input_event> handleEvent(const struct timeval& time, bool positive);

  std::optional<struct input_event> tick();

  std::optional<struct timeval> timeout();

  std::optional<std::chrono::microseconds> next_tick_time();

  void handleRelXEvent(const struct timeval& time, int value);

  void handleRelYEvent(const struct timeval& time, int value);

private:
  double smoothSpeed(const std::chrono::microseconds event_interval);

  Options options_;

  double tick_interval_;
  double min_delta_;
  double min_delta_decrease_per_tick_;
  double max_delta_decrease_per_tick_;
  double initial_delta_;
  double alpha_;
  double max_delta_increase_;
  double max_delta_decrease_;
  double delta_decrease_per_braking_;
  double braking_cut_off_delta_;
  double delta_decrease_per_mouse_movement_;
  double mouse_movement_braking_cut_off_delta_;

  std::vector<std::chrono::microseconds> event_intervals_;
  std::chrono::microseconds last_event_time_{ 0 };
  std::chrono::microseconds next_tick_time_{ 0 };
  std::chrono::microseconds last_brake_stop_time_{ 0 };
  std::chrono::microseconds last_mouse_movement_time_{ 0 };
  bool positive_ = false;
  double delta_ = 0;
  double deviation_ = 0;
  int total_delta_ = 0;
  int braking_times_ = 0;
  bool mouse_movement_dejitter_ = true;
  int mouse_movement_x_ = 0;
  int mouse_movement_y_ = 0;
  bool free_spin_ = false;
};

}  // namespace smooth_scroll
