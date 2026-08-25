// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Wayne6530

#pragma once

#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <vector>

#include <linux/input.h>

#include "mouse_movement_buffer.h"

namespace smooth_scroll
{

class WheelSmoother
{
  enum class AutoScrollState
  {
    Inactive,
    Held,
    Latched,
    ExitHeld,
  };

public:
  enum class SmoothMode
  {
    Speed = 0,
    Distance = 1,
    Hybrid = 2,
  };

  enum class DragViewActivationMode
  {
    Scrolling = 0,
    Always = 1,
  };

  enum class AutoScrollActivationMode
  {
    Scrolling = 0,
    Always = 1,
  };

  enum class AutoScrollAxisMode
  {
    Vertical = 0,
    Horizontal = 1,
    Omnidirectional = 2,
  };

  enum class DragViewButtonResult
  {
    Passthrough,
    Handled,
    ReplayClick,
  };

  enum class AutoScrollButtonResult
  {
    Passthrough,
    Handled,
    ReplayClick,
  };

  enum class ReportResult
  {
    None,
    ScrollStopped,
    AutoScrollOffsetChanged,
  };

  struct TickResult
  {
    std::array<struct input_event, 2> events{};
    std::size_t count = 0;
  };

  struct Options
  {
    SmoothMode smooth_mode = SmoothMode::Hybrid;
    int wheel_tick_distance = 120;
    int tick_interval_microseconds = 2000;

    double min_deceleration = 1420;
    double max_deceleration = 6000;
    double initial_speed = 600;
    double speed_factor = 40;
    int speed_smooth_window_microseconds = 200000;
    double max_speed_change_lowerbound = 512;
    double min_speed_change_upperbound = 1024;
    double min_speed_change_ratio = 0.0625;
    double max_speed_change_ratio = 1;
    double damping = 3.1;

    bool use_reverse_scroll_braking = true;
    int max_reverse_scroll_braking_microseconds = 100000;
    int max_reverse_scroll_braking_times = 3;
    int reverse_scroll_intent_window_microseconds = 200000;

    bool use_mouse_movement_braking = true;
    int max_mouse_movement_distance = 30;
    int mouse_movement_window_milliseconds = 20;
    int mouse_movement_delay_microseconds = 100000;

    DragViewActivationMode drag_view_activation_mode = DragViewActivationMode::Scrolling;
    int drag_view_click_timeout_milliseconds = 200;
    int drag_view_speed = 3;

    AutoScrollActivationMode auto_scroll_activation_mode = AutoScrollActivationMode::Scrolling;
    AutoScrollAxisMode auto_scroll_axis_mode = AutoScrollAxisMode::Omnidirectional;
    int auto_scroll_deadzone = 8;
    int auto_scroll_click_timeout_milliseconds = 200;
    double auto_scroll_speed_factor = 50;
    double auto_scroll_max_speed = 6000;
  };

  explicit WheelSmoother(const Options& options);

  WheelSmoother(const WheelSmoother&) = delete;
  WheelSmoother& operator=(const WheelSmoother&) = delete;

  WheelSmoother(WheelSmoother&&) = delete;
  WheelSmoother& operator=(WheelSmoother&&) = delete;

  void stop() noexcept;

  void hardReset() noexcept;

  bool handleFreeSpinButton(int value) noexcept;

  DragViewButtonResult handleDragViewButton(const struct timeval& time, int value) noexcept;

  AutoScrollButtonResult handleAutoScrollButton(const struct timeval& time, int value) noexcept;

  void handleOrdinaryButton() noexcept;

  std::optional<struct input_event> handleEvent(const struct timeval& time, bool positive, bool horizontal);

  TickResult tick() noexcept;

  std::optional<struct timeval> timeout() const noexcept;

  std::optional<std::chrono::microseconds> next_tick_time() const noexcept;

  [[nodiscard]] bool handleRelXEvent(struct input_event& ev) noexcept;

  [[nodiscard]] bool handleRelYEvent(struct input_event& ev) noexcept;

  [[nodiscard]] ReportResult handleReportEvent(const struct timeval& time) noexcept;

  [[nodiscard]] bool positive() const noexcept
  {
    return positive_;
  }

  [[nodiscard]] bool horizontal() const noexcept
  {
    return horizontal_;
  }

  [[nodiscard]] double speed() const noexcept
  {
    return speed_;
  }

  [[nodiscard]] bool free_spin() const noexcept
  {
    return free_spin_;
  }

  [[nodiscard]] bool drag_view() const noexcept
  {
    return drag_view_;
  }

  [[nodiscard]] bool auto_scroll() const noexcept
  {
    return auto_scroll_state_ != AutoScrollState::Inactive;
  }

  [[nodiscard]] int64_t auto_scroll_offset_x() const noexcept
  {
    return auto_scroll_offset_x_;
  }

  [[nodiscard]] int64_t auto_scroll_offset_y() const noexcept
  {
    return auto_scroll_offset_y_;
  }

  [[nodiscard]] bool auto_scroll_horizontal_enabled() const noexcept
  {
    return options_.auto_scroll_axis_mode != AutoScrollAxisMode::Vertical;
  }

  [[nodiscard]] bool auto_scroll_vertical_enabled() const noexcept
  {
    return options_.auto_scroll_axis_mode != AutoScrollAxisMode::Horizontal;
  }

private:
  struct input_event makeWheelEvent(const struct timeval& time, int round_delta) const noexcept;

  std::optional<struct input_event> handleSpeedEvent(const struct timeval& time, bool positive, bool horizontal,
                                                     std::chrono::microseconds event_time);

  std::optional<struct input_event> handleDistanceEvent(const struct timeval& time, bool positive, bool horizontal,
                                                        std::chrono::microseconds event_time);

  std::optional<struct input_event> handleHybridEvent(const struct timeval& time, bool positive, bool horizontal,
                                                      std::chrono::microseconds event_time);

  std::optional<struct input_event> tickSpeed() noexcept;

  std::optional<struct input_event> tickDistance() noexcept;

  std::optional<struct input_event> tickHybrid() noexcept;

  TickResult tickAutoScroll() noexcept;

  void stopScroll() noexcept;

  void stopAutoScroll() noexcept;

  void resetAutoScrollMotion() noexcept;

  [[nodiscard]] bool auto_scroll_button_held() const noexcept
  {
    return auto_scroll_state_ == AutoScrollState::Held || auto_scroll_state_ == AutoScrollState::ExitHeld;
  }

  [[nodiscard]] bool scrollActive() const noexcept;

  [[nodiscard]] bool scrolling() const noexcept
  {
    return scrollActive() || auto_scroll();
  }

  [[nodiscard]] bool autoScrollMoving() const noexcept;

  [[nodiscard]] double autoScrollSpeedForOffset(int64_t offset) const noexcept;

  double speedForDistance(double distance) const noexcept;

  double smoothSpeed(const std::chrono::microseconds event_interval);

  Options options_;

  double tick_interval_;
  double inv_tick_interval_;
  double min_delta_decrease_per_tick_;
  double max_delta_decrease_per_tick_;
  double initial_delta_;
  double alpha_;
  double max_delta_change_lowerbound_;
  double min_delta_change_upperbound_;
  double distance_curve_low_speed_;
  double distance_curve_high_speed_squared_;
  double distance_curve_low_distance_;
  double distance_curve_high_distance_;
  int squared_max_mouse_movement_distance_;
  int64_t auto_scroll_max_offset_ = 0;
  MouseMovementBuffer mouse_movement_buffer_;
  std::vector<double> max_delta_braking_times_;

  std::vector<std::chrono::microseconds> event_intervals_;
  std::chrono::microseconds last_event_time_{ 0 };
  std::chrono::microseconds next_tick_time_{ 0 };
  std::chrono::microseconds last_brake_stop_time_{ 0 };
  std::chrono::microseconds drag_view_press_time_{ 0 };
  std::chrono::microseconds auto_scroll_press_time_{ 0 };
  bool positive_ = false;
  bool horizontal_ = false;
  double delta_ = 0;
  double distance_remaining_ = 0;
  double speed_ = 0;
  double deviation_ = 0;
  double free_spin_deviation_ = 0;
  int total_delta_ = 0;
  int braking_times_ = 0;
  int rel_x_ = 0;
  int rel_y_ = 0;
  int64_t auto_scroll_offset_x_ = 0;
  int64_t auto_scroll_offset_y_ = 0;
  double auto_scroll_deviation_x_ = 0;
  double auto_scroll_deviation_y_ = 0;
  bool free_spin_ = false;
  bool drag_view_ = false;
  AutoScrollState auto_scroll_state_ = AutoScrollState::Inactive;
};

}  // namespace smooth_scroll
