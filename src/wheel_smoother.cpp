// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Wayne6530

#include "wheel_smoother.h"

#include <algorithm>
#include <cmath>

#include <spdlog/spdlog.h>

namespace smooth_scroll
{

WheelSmoother::WheelSmoother(const Options& options)
  : options_{ options }
  , tick_interval_{ static_cast<double>(options.tick_interval_microseconds) / 1.e6 }
  , inv_tick_interval_{ 1.0 / tick_interval_ }
  , min_delta_decrease_per_tick_{ options.min_deceleration * tick_interval_ * tick_interval_ }
  , max_delta_decrease_per_tick_{ options.max_deceleration * tick_interval_ * tick_interval_ }
  , initial_delta_{ options.initial_speed * tick_interval_ }
  , alpha_{ std::exp(-options.damping * tick_interval_) }
  , max_delta_change_lowerbound_{ options.max_speed_change_lowerbound * tick_interval_ }
  , min_delta_change_upperbound_{ options.min_speed_change_upperbound * tick_interval_ }
  , distance_curve_low_speed_{ options.min_deceleration / options.damping }
  , distance_curve_high_speed_squared_{ options.max_deceleration * options.max_deceleration /
                                        (options.damping * options.damping) }
  , distance_curve_low_distance_{ distance_curve_low_speed_ * distance_curve_low_speed_ /
                                  (2.0 * options.min_deceleration) }
  , distance_curve_high_distance_{ distance_curve_low_distance_ +
                                   (options.max_deceleration - options.min_deceleration) /
                                       (options.damping * options.damping) }
  , squared_max_mouse_movement_distance_(options.max_mouse_movement_distance * options.max_mouse_movement_distance)
  , mouse_movement_buffer_{ std::chrono::milliseconds(options.mouse_movement_window_milliseconds) }
{
  SPDLOG_DEBUG("tick interval {}s alpha {}", tick_interval_, alpha_);

  if (options.use_reverse_scroll_braking)
  {
    max_delta_braking_times_.reserve(options.max_reverse_scroll_braking_times);

    double max_delta = initial_delta_;
    max_delta_braking_times_.push_back(max_delta);

    for (int i = 0; i < options.max_reverse_scroll_braking_times; ++i)
    {
      max_delta += std::max(max_delta * options.max_speed_change_ratio, min_delta_change_upperbound_);
      max_delta_braking_times_.push_back(max_delta);
    }
  }

  event_intervals_.reserve(16);
}

void WheelSmoother::stop() noexcept
{
  stopScroll();
  braking_times_ = 0;
}

bool WheelSmoother::handleFreeSpinButton(int value) noexcept
{
  if (delta_ != 0 && value == 1)
  {
    free_spin_ = true;
    return true;
  }

  if (free_spin_)
  {
    if (value == 0)
    {
      free_spin_ = false;
    }
    return true;
  }

  return false;
}

WheelSmoother::DragViewButtonResult WheelSmoother::handleDragViewButton(const struct timeval& time, int value) noexcept
{
  if (!drag_view_ && value == 1 &&
      (delta_ != 0 || options_.drag_view_activation_mode == DragViewActivationMode::Always))
  {
    drag_view_ = true;
    drag_view_press_time_ =
        std::chrono::seconds{ time.tv_sec } + std::chrono::microseconds{ time.tv_usec };
    stopScroll();
    return DragViewButtonResult::Handled;
  }

  if (drag_view_)
  {
    if (value == 0)
    {
      drag_view_ = false;

      const std::chrono::microseconds release_time =
          std::chrono::seconds{ time.tv_sec } + std::chrono::microseconds{ time.tv_usec };
      const std::chrono::microseconds click_timeout =
          std::chrono::milliseconds{ options_.drag_view_click_timeout_milliseconds };
      const std::chrono::microseconds press_duration = release_time - drag_view_press_time_;

      if (press_duration >= std::chrono::microseconds::zero() && press_duration < click_timeout)
      {
        return DragViewButtonResult::ReplayClick;
      }
    }
    return DragViewButtonResult::Handled;
  }

  return DragViewButtonResult::Passthrough;
}

std::optional<struct input_event> WheelSmoother::handleEvent(const struct timeval& time, bool positive, bool horizontal)
{
  if (drag_view_)
  {
    return std::nullopt;
  }

  std::chrono::microseconds event_time =
      std::chrono::seconds{ time.tv_sec } + std::chrono::microseconds{ time.tv_usec };

  if (options_.smooth_mode == SmoothMode::Distance)
  {
    return handleDistanceEvent(time, positive, horizontal, event_time);
  }

  return handleSpeedEvent(time, positive, horizontal, event_time);
}

std::optional<struct input_event> WheelSmoother::handleSpeedEvent(const struct timeval& time, bool positive,
                                                                  bool horizontal, std::chrono::microseconds event_time)
{
  if (horizontal_ != horizontal)
  {
    stopScroll();
    braking_times_ = 0;
  }

  if (options_.use_reverse_scroll_braking)
  {
    if (positive == positive_)
    {
      braking_times_ = 0;
    }
    else
    {
      if (delta_ != 0)
      {
        SPDLOG_DEBUG("reverse scroll stop");
        event_intervals_.clear();
        last_event_time_ = event_time;
        last_brake_stop_time_ = event_time;
        stopScroll();
        braking_times_ = 1;

        return std::nullopt;
      }

      // delta_ == 0
      if (braking_times_)
      {
        if (event_time <
                last_brake_stop_time_ + std::chrono::microseconds{ options_.max_reverse_scroll_braking_microseconds } &&
            braking_times_ < options_.max_reverse_scroll_braking_times)
        {
          SPDLOG_DEBUG("braking dejitter");
          event_intervals_.push_back(event_time - last_event_time_);
          last_event_time_ = event_time;
          ++braking_times_;
          return std::nullopt;
        }

        double speed = smoothSpeed(event_time - last_event_time_);

        last_event_time_ = event_time;
        next_tick_time_ = event_time + std::chrono::microseconds{ options_.tick_interval_microseconds };

        positive_ = positive;

        delta_ = std::clamp(speed * tick_interval_, initial_delta_, max_delta_braking_times_[braking_times_]);
        speed_ = delta_ * inv_tick_interval_;
        braking_times_ = 0;

        SPDLOG_DEBUG("initial speed {:.2f}", speed_);

        int round_delta = std::round(delta_);
        deviation_ = delta_ - round_delta;

        total_delta_ = round_delta;

        struct input_event ev;
        ev.time = time;
        ev.type = EV_REL;
        ev.code = horizontal_ ? REL_HWHEEL_HI_RES : REL_WHEEL_HI_RES;
        ev.value = positive_ ? round_delta : -round_delta;

        return ev;
      }
    }
  }

  if (delta_ == 0)
  {
    event_intervals_.clear();
    last_event_time_ = event_time;
    next_tick_time_ = event_time + std::chrono::microseconds{ options_.tick_interval_microseconds };

    positive_ = positive;
    horizontal_ = horizontal;
    delta_ = initial_delta_;
    speed_ = delta_ * inv_tick_interval_;

    SPDLOG_DEBUG("initial speed {:.2f}", speed_);

    int round_delta = std::round(delta_);
    deviation_ = delta_ - round_delta;

    total_delta_ = round_delta;

    struct input_event ev;
    ev.time = time;
    ev.type = EV_REL;
    ev.code = horizontal_ ? REL_HWHEEL_HI_RES : REL_WHEEL_HI_RES;
    ev.value = positive_ ? round_delta : -round_delta;

    return ev;
  }

  const double speed = smoothSpeed(event_time - last_event_time_);
  const double min_delta_change = std::min(delta_ * options_.min_speed_change_ratio, max_delta_change_lowerbound_);
  const double max_delta_change = std::max(delta_ * options_.max_speed_change_ratio, min_delta_change_upperbound_);

  double delta = std::clamp(speed * tick_interval_, delta_ + min_delta_change, delta_ + max_delta_change);

  last_event_time_ = event_time;
  delta_ = delta < initial_delta_ ? initial_delta_ : delta;
  speed_ = delta_ * inv_tick_interval_;

  SPDLOG_DEBUG("set speed: actual {:.2f} target {:.2f}", speed_, speed);

  return std::nullopt;
}

std::optional<struct input_event> WheelSmoother::handleDistanceEvent(const struct timeval& time, bool positive,
                                                                     bool horizontal,
                                                                     std::chrono::microseconds event_time)
{
  if (horizontal_ != horizontal)
  {
    stopScroll();
    braking_times_ = 0;
  }

  auto start_distance_scroll = [&](int distance_ticks) -> std::optional<struct input_event> {
    last_event_time_ = event_time;
    next_tick_time_ = event_time + std::chrono::microseconds{ options_.tick_interval_microseconds };

    positive_ = positive;
    horizontal_ = horizontal;
    deviation_ = 0;
    total_delta_ = 0;

    const int initial_distance = options_.wheel_tick_distance * distance_ticks;
    delta_ = initial_distance;
    speed_ = speedForDistance(delta_);

    SPDLOG_DEBUG("distance mode target speed {:.2f}, remaining {:.2f}", speed_, delta_);

    double desired_delta = std::min(speed_ * tick_interval_, delta_);
    int round_delta = std::min(static_cast<int>(std::round(desired_delta)), initial_distance);

    deviation_ = desired_delta - round_delta;
    delta_ -= desired_delta;

    if (round_delta <= 0)
    {
      return std::nullopt;
    }

    total_delta_ = round_delta;

    struct input_event ev;
    ev.time = time;
    ev.type = EV_REL;
    ev.code = horizontal_ ? REL_HWHEEL_HI_RES : REL_WHEEL_HI_RES;
    ev.value = positive_ ? round_delta : -round_delta;

    return ev;
  };

  if (options_.use_reverse_scroll_braking)
  {
    if (positive == positive_)
    {
      braking_times_ = 0;
    }
    else
    {
      if (delta_ != 0)
      {
        SPDLOG_DEBUG("distance reverse scroll stop");
        last_event_time_ = event_time;
        last_brake_stop_time_ = event_time;
        stopScroll();
        braking_times_ = 1;

        return std::nullopt;
      }

      if (braking_times_)
      {
        if (event_time <
                last_brake_stop_time_ + std::chrono::microseconds{ options_.max_reverse_scroll_braking_microseconds } &&
            braking_times_ < options_.max_reverse_scroll_braking_times)
        {
          SPDLOG_DEBUG("distance braking dejitter");
          last_event_time_ = event_time;
          ++braking_times_;
          return std::nullopt;
        }

        int distance_ticks = braking_times_ + 1;
        braking_times_ = 0;
        return start_distance_scroll(distance_ticks);
      }
    }
  }
  else if (delta_ != 0 && positive != positive_)
  {
    stopScroll();
  }

  if (delta_ == 0)
  {
    return start_distance_scroll(1);
  }

  delta_ += options_.wheel_tick_distance;
  speed_ = speedForDistance(delta_);

  SPDLOG_DEBUG("distance mode target speed {:.2f}, remaining {:.2f}", speed_, delta_);

  last_event_time_ = event_time;
  return std::nullopt;
}

std::optional<struct input_event> WheelSmoother::tick() noexcept
{
  if (options_.smooth_mode == SmoothMode::Distance)
  {
    return tickDistance();
  }

  return tickSpeed();
}

std::optional<struct input_event> WheelSmoother::tickSpeed() noexcept
{
  if (delta_ == 0)
  {
    return std::nullopt;
  }

  if (!free_spin_)
  {
    double max_delta = delta_ - min_delta_decrease_per_tick_;
    double min_delta = delta_ - max_delta_decrease_per_tick_;

    delta_ *= alpha_;

    if (delta_ > max_delta)
    {
      delta_ = max_delta;
    }

    if (delta_ < min_delta)
    {
      delta_ = min_delta;
    }

    if (delta_ < 0)
    {
      SPDLOG_DEBUG("damping stop, total {}", total_delta_);

      stopScroll();
      return std::nullopt;
    }

    speed_ = delta_ * inv_tick_interval_;
    SPDLOG_TRACE("tick speed {:.2f} deceleration {:.2f}", speed_,
                 (max_delta + min_delta_decrease_per_tick_ - delta_) / (tick_interval_ * tick_interval_));
  }

  std::chrono::microseconds current_tick_time = next_tick_time_;
  next_tick_time_ += std::chrono::microseconds{ options_.tick_interval_microseconds };

  int round_delta = std::round(delta_ + deviation_);
  deviation_ = delta_ + deviation_ - round_delta;

  if (round_delta == 0)
  {
    return std::nullopt;
  }

  total_delta_ += round_delta;

  struct input_event ev;
  ev.time.tv_sec = std::chrono::duration_cast<std::chrono::seconds>(current_tick_time).count();
  ev.time.tv_usec = (current_tick_time - std::chrono::seconds{ ev.time.tv_sec }).count();
  ev.type = EV_REL;
  ev.code = horizontal_ ? REL_HWHEEL_HI_RES : REL_WHEEL_HI_RES;
  ev.value = positive_ ? round_delta : -round_delta;

  return ev;
}

std::optional<struct input_event> WheelSmoother::tickDistance() noexcept
{
  if (delta_ == 0)
  {
    stopScroll();
    return std::nullopt;
  }

  std::chrono::microseconds current_tick_time = next_tick_time_;
  next_tick_time_ += std::chrono::microseconds{ options_.tick_interval_microseconds };

  speed_ = speedForDistance(delta_);
  double desired_delta = speed_ * tick_interval_;
  if (free_spin_)
  {
    int round_delta = std::round(desired_delta);
    if (round_delta <= 0)
    {
      return std::nullopt;
    }

    struct input_event ev;
    ev.time.tv_sec = std::chrono::duration_cast<std::chrono::seconds>(current_tick_time).count();
    ev.time.tv_usec = (current_tick_time - std::chrono::seconds{ ev.time.tv_sec }).count();
    ev.type = EV_REL;
    ev.code = horizontal_ ? REL_HWHEEL_HI_RES : REL_WHEEL_HI_RES;
    ev.value = positive_ ? round_delta : -round_delta;

    total_delta_ += round_delta;
    return ev;
  }

  desired_delta = std::min(desired_delta, delta_);

  const int remaining_delta = static_cast<int>(std::round(delta_ + deviation_));
  int round_delta = std::clamp(static_cast<int>(std::round(desired_delta + deviation_)), 0, remaining_delta);

  deviation_ += desired_delta - round_delta;
  delta_ -= desired_delta;

  if (round_delta <= 0)
  {
    if (delta_ <= 0)
    {
      SPDLOG_DEBUG("distance damping stop, total {}", total_delta_);
      stopScroll();
    }
    return std::nullopt;
  }

  struct input_event ev;
  ev.time.tv_sec = std::chrono::duration_cast<std::chrono::seconds>(current_tick_time).count();
  ev.time.tv_usec = (current_tick_time - std::chrono::seconds{ ev.time.tv_sec }).count();
  ev.type = EV_REL;
  ev.code = horizontal_ ? REL_HWHEEL_HI_RES : REL_WHEEL_HI_RES;
  ev.value = positive_ ? round_delta : -round_delta;

  total_delta_ += round_delta;

  return ev;
}

std::optional<struct timeval> WheelSmoother::timeout() const noexcept
{
  if (delta_ == 0)
  {
    return std::nullopt;
  }

  std::chrono::microseconds now =
      std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::system_clock::now().time_since_epoch());

  if (now >= next_tick_time_)
  {
    struct timeval timeout;
    timeout.tv_sec = 0;
    timeout.tv_usec = 0;
    return timeout;
  }

  struct timeval timeout;
  timeout.tv_sec = 0;
  timeout.tv_usec = (next_tick_time_ - now).count();

  while (timeout.tv_usec >= 1'000'000)
  {
    timeout.tv_sec += 1;
    timeout.tv_usec -= 1'000'000;
  }

  return timeout;
}

std::optional<std::chrono::microseconds> WheelSmoother::next_tick_time() const noexcept
{
  if (delta_ == 0)
  {
    return std::nullopt;
  }

  return next_tick_time_;
}

void WheelSmoother::handleRelXEvent(struct input_event& ev) noexcept
{
  if (drag_view_)
  {
    ev.code = REL_HWHEEL_HI_RES;
    ev.value = options_.drag_view_speed * ev.value;
    return;
  }

  rel_x_ = ev.value;
}

void WheelSmoother::handleRelYEvent(struct input_event& ev) noexcept
{
  if (drag_view_)
  {
    ev.code = REL_WHEEL_HI_RES;
    ev.value = -options_.drag_view_speed * ev.value;
    return;
  }

  rel_y_ = ev.value;
}

bool WheelSmoother::handleReportEvent(const struct timeval& time) noexcept
{
  if (rel_x_ == 0 && rel_y_ == 0)
  {
    return false;
  }

  if (delta_ != 0 && options_.use_mouse_movement_braking && !free_spin_)
  {
    std::chrono::microseconds event_time =
        std::chrono::seconds{ time.tv_sec } + std::chrono::microseconds{ time.tv_usec };

    if (event_time > last_event_time_ + std::chrono::microseconds{ options_.mouse_movement_delay_microseconds })
    {
      auto result =
          mouse_movement_buffer_.add(std::chrono::duration_cast<std::chrono::milliseconds>(event_time), rel_x_, rel_y_);

      int squared_distance = result.x * result.x + result.y * result.y;
      if (squared_distance > squared_max_mouse_movement_distance_)
      {
        SPDLOG_DEBUG("movement stop");
        stopScroll();

        rel_x_ = 0;
        rel_y_ = 0;
        return true;
      }
    }
  }

  rel_x_ = 0;
  rel_y_ = 0;
  return false;
}

void WheelSmoother::stopScroll() noexcept
{
  delta_ = 0;
  speed_ = 0;
}

double WheelSmoother::speedForDistance(double distance) const noexcept
{
  if (distance <= distance_curve_low_distance_)
  {
    return std::sqrt(2.0 * options_.min_deceleration * distance);
  }

  if (distance <= distance_curve_high_distance_)
  {
    return distance_curve_low_speed_ + options_.damping * (distance - distance_curve_low_distance_);
  }

  return std::sqrt(distance_curve_high_speed_squared_ +
                   2.0 * options_.max_deceleration * (distance - distance_curve_high_distance_));
}

double WheelSmoother::smoothSpeed(const std::chrono::microseconds event_interval)
{
  const std::chrono::microseconds speed_smooth_window{ options_.speed_smooth_window_microseconds };

  double num_event_intervals = 1;
  std::chrono::microseconds duration = event_interval;

  if (event_interval > speed_smooth_window)
  {
    event_intervals_.clear();
  }
  else
  {
    for (auto iter = event_intervals_.rbegin(); iter != event_intervals_.rend(); ++iter)
    {
      if (*iter + duration > speed_smooth_window)
      {
        num_event_intervals += std::chrono::duration<double>(speed_smooth_window - duration).count() /
                               std::chrono::duration<double>(*iter).count();
        duration = speed_smooth_window;
        break;
      }

      duration += *iter;
      num_event_intervals += 1;
    }
    event_intervals_.push_back(event_interval);
  }

  return options_.speed_factor * num_event_intervals / std::chrono::duration<double>(duration).count();
}

}  // namespace smooth_scroll
