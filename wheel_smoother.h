#pragma once

#include <chrono>
#include <optional>

#include <linux/input.h>

namespace smooth_scroll
{

class WheelSmoother
{
public:
  struct Options
  {
    int tick_interval_microseconds = 16667;

    double min_speed = 200;
    double initial_speed = 600;
    double speed_factor = 80;
    double max_speed_increase_per_wheel_event = 1000;
    double max_speed_decrease_per_wheel_event = 0;
    double damping = 3.1;

    bool use_braking = true;
    int braking_dejitter_microseconds = 100000;
    double braking_cut_off_speed = 1000;
    double speed_decrease_per_braking = std::numeric_limits<double>::infinity();
  };

  explicit WheelSmoother(const Options& options);

  WheelSmoother(const WheelSmoother&) = delete;
  WheelSmoother& operator=(const WheelSmoother&) = delete;

  WheelSmoother(WheelSmoother&&) = delete;
  WheelSmoother& operator=(WheelSmoother&&) = delete;

  void stop();

  std::optional<struct input_event> handleEvent(const struct timeval& time, bool positive);

  std::optional<struct input_event> tick();

  std::optional<struct timeval> timeout();

  std::optional<std::chrono::microseconds> next_tick_time();

private:
  Options options_;

  double tick_interval_;
  double min_delta_;
  double initial_delta_;
  double alpha_;
  double max_delta_increase_;
  double max_delta_decrease_;
  double delta_decrease_per_braking_;
  double braking_cut_off_delta_;

  std::chrono::microseconds last_event_time_{ 0 };
  std::chrono::microseconds next_tick_time_{ 0 };
  std::chrono::microseconds last_brake_stop_time_{ 0 };
  bool positive_ = false;
  double delta_ = 0;
  double deviation_ = 0;
};

}  // namespace smooth_scroll
