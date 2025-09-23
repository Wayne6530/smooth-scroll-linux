// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Wayne6530

#include "wheel_smoother.h"

#include <spdlog/spdlog.h>

namespace smooth_scroll
{

WheelSmoother::WheelSmoother(const Options& options)
  : options_{ options }
  , tick_interval_{ static_cast<double>(options.tick_interval_microseconds) / 1.e6 }
  , min_delta_{ options.min_speed * tick_interval_ }
  , min_delta_decrease_per_tick_{ options.min_deceleration * tick_interval_ * tick_interval_ }
  , max_delta_decrease_per_tick_{ options.max_deceleration * tick_interval_ * tick_interval_ }
  , initial_delta_{ options.initial_speed * tick_interval_ }
  , alpha_{ std::exp(-options.damping * tick_interval_) }
  , max_delta_increase_{ options.max_speed_increase_per_wheel_event * tick_interval_ }
  , max_delta_decrease_{ options.max_speed_decrease_per_wheel_event * tick_interval_ }
  , delta_decrease_per_braking_{ options.speed_decrease_per_braking * tick_interval_ }
  , braking_cut_off_delta_{ options_.braking_cut_off_speed * tick_interval_ }
  , delta_decrease_per_mouse_movement_{ options.speed_decrease_per_mouse_movement * tick_interval_ }
  , mouse_movement_braking_cut_off_delta_{ options.mouse_movement_braking_cut_off_speed * tick_interval_ }
{
  SPDLOG_DEBUG("tick interval {}s alpha {}", tick_interval_, alpha_);
}

void WheelSmoother::stop()
{
  if (delta_ != 0)
  {
    SPDLOG_DEBUG("click stop");
    delta_ = 0;
  }
}

bool WheelSmoother::setFreeSpin(bool enabled)
{
  if (free_spin_ || delta_ != 0)
  {
    free_spin_ = enabled;
    return true;
  }

  return false;
}

std::optional<struct input_event> WheelSmoother::handleEvent(const struct timeval& time, bool positive)
{
  mouse_movement_dejitter_ = true;
  mouse_movement_x_ = 0;
  mouse_movement_y_ = 0;

  std::chrono::microseconds event_time =
      std::chrono::seconds{ time.tv_sec } + std::chrono::microseconds{ time.tv_usec };

  if (options_.use_braking)
  {
    if (positive == positive_)
    {
      braking_times_ = 0;
    }
    else
    {
      if (delta_ != 0)
      {
        delta_ -= delta_decrease_per_braking_;

        if (delta_ < braking_cut_off_delta_)
        {
          SPDLOG_DEBUG("braking stop");
          event_intervals_.clear();
          last_event_time_ = event_time;
          last_brake_stop_time_ = event_time;
          delta_ = 0;
          braking_times_ = 1;
        }
        else
        {
          SPDLOG_DEBUG("braking");
        }

        return {};
      }

      // delta_ == 0
      if (braking_times_)
      {
        if (event_time < last_brake_stop_time_ + std::chrono::microseconds{ options_.braking_dejitter_microseconds } &&
            braking_times_ < options_.max_braking_times)
        {
          SPDLOG_DEBUG("braking dejitter");
          event_intervals_.push_back(event_time - last_event_time_);
          last_event_time_ = event_time;
          ++braking_times_;
          return {};
        }

        double speed = smoothSpeed(event_time - last_event_time_);

        last_event_time_ = event_time;
        next_tick_time_ = event_time + std::chrono::microseconds{ options_.tick_interval_microseconds };

        positive_ = positive;

        delta_ =
            std::clamp(speed * tick_interval_, initial_delta_, initial_delta_ + braking_times_ * max_delta_increase_);
        braking_times_ = 0;

        SPDLOG_DEBUG("initial speed {:.2f}", delta_ / tick_interval_);

        int round_delta = std::round(delta_);
        deviation_ = delta_ - round_delta;

        total_delta_ = round_delta;

        struct input_event ev;
        ev.time = time;
        ev.type = EV_REL;
        ev.code = REL_WHEEL_HI_RES;
        ev.value = positive_ ? round_delta : -round_delta;

        return ev;
      }
    }
  }

  if (delta_ == 0 || positive != positive_)
  {
    event_intervals_.clear();
    last_event_time_ = event_time;
    next_tick_time_ = event_time + std::chrono::microseconds{ options_.tick_interval_microseconds };

    positive_ = positive;
    delta_ = initial_delta_;

    SPDLOG_DEBUG("initial speed {:.2f}", delta_ / tick_interval_);

    int round_delta = std::round(delta_);
    deviation_ = delta_ - round_delta;

    total_delta_ = round_delta;

    struct input_event ev;
    ev.time = time;
    ev.type = EV_REL;
    ev.code = REL_WHEEL_HI_RES;
    ev.value = positive_ ? round_delta : -round_delta;

    return ev;
  }

  double speed = smoothSpeed(event_time - last_event_time_);
  double delta = std::clamp(speed * tick_interval_, delta_ - max_delta_decrease_, delta_ + max_delta_increase_);

  last_event_time_ = event_time;

  positive_ = positive;
  delta_ = delta < initial_delta_ ? initial_delta_ : delta;

  SPDLOG_DEBUG("set speed: actual {:.2f} target {:.2f}", delta_ / tick_interval_, speed);

  return {};
}

std::optional<struct input_event> WheelSmoother::tick()
{
  if (delta_ == 0)
  {
    return {};
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

    if (delta_ < min_delta_)
    {
      SPDLOG_DEBUG("damping stop, total {}", total_delta_);

      delta_ = 0;
      return {};
    }

    SPDLOG_TRACE("tick speed {:.2f} deceleration {:.2f}", delta_ / tick_interval_,
                 (max_delta + min_delta_decrease_per_tick_ - delta_) / (tick_interval_ * tick_interval_));
  }

  std::chrono::microseconds current_tick_time = next_tick_time_;
  next_tick_time_ += std::chrono::microseconds{ options_.tick_interval_microseconds };

  int round_delta = std::round(delta_ + deviation_);
  deviation_ = delta_ + deviation_ - round_delta;

  if (round_delta == 0)
  {
    return {};
  }

  total_delta_ += round_delta;

  struct input_event ev;
  ev.time.tv_sec = std::chrono::duration_cast<std::chrono::seconds>(current_tick_time).count();
  ev.time.tv_usec = (current_tick_time - std::chrono::seconds{ ev.time.tv_sec }).count();
  ev.type = EV_REL;
  ev.code = REL_WHEEL_HI_RES;
  ev.value = positive_ ? round_delta : -round_delta;

  return ev;
}

std::optional<struct timeval> WheelSmoother::timeout()
{
  if (delta_ == 0)
  {
    return {};
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

std::optional<std::chrono::microseconds> WheelSmoother::next_tick_time()
{
  if (delta_ == 0)
  {
    return {};
  }

  return next_tick_time_;
}

void WheelSmoother::handleRelXEvent(const struct timeval& time, int value)
{
  if (delta_ == 0 || !options_.use_mouse_movement_braking)
  {
    return;
  }

  std::chrono::microseconds event_time =
      std::chrono::seconds{ time.tv_sec } + std::chrono::microseconds{ time.tv_usec };

  if (event_time >
      last_mouse_movement_time_ + std::chrono::microseconds{ options_.max_mouse_movement_event_interval_microseconds })
  {
    last_mouse_movement_time_ = event_time;

    SPDLOG_TRACE("mouse movement stop, enable dejitter");

    mouse_movement_dejitter_ = true;
    mouse_movement_x_ = value;
    mouse_movement_y_ = 0;
    return;
  }

  last_mouse_movement_time_ = event_time;

  if (mouse_movement_dejitter_)
  {
    mouse_movement_x_ += value;
    SPDLOG_TRACE("mouse movement x {}", mouse_movement_x_);

    if (mouse_movement_x_ > options_.mouse_movement_dejitter_distance)
    {
      mouse_movement_dejitter_ = false;
      value = mouse_movement_x_ - options_.mouse_movement_dejitter_distance;
    }
    else if (mouse_movement_x_ < -options_.mouse_movement_dejitter_distance)
    {
      mouse_movement_dejitter_ = false;
      value = mouse_movement_x_ + options_.mouse_movement_dejitter_distance;
    }
    else
    {
      return;
    }
  }

  delta_ -= delta_decrease_per_mouse_movement_ * std::abs(value);

  if (delta_ < mouse_movement_braking_cut_off_delta_)
  {
    SPDLOG_DEBUG("mouse movement braking stop");
    delta_ = 0;
  }
  else
  {
    SPDLOG_TRACE("mouse movement braking");
  }
}

void WheelSmoother::handleRelYEvent(const struct timeval& time, int value)
{
  if (delta_ == 0 || !options_.use_mouse_movement_braking)
  {
    return;
  }

  std::chrono::microseconds event_time =
      std::chrono::seconds{ time.tv_sec } + std::chrono::microseconds{ time.tv_usec };

  if (event_time >
      last_mouse_movement_time_ + std::chrono::microseconds{ options_.max_mouse_movement_event_interval_microseconds })
  {
    last_mouse_movement_time_ = event_time;

    SPDLOG_TRACE("mouse movement stop, enable dejitter");

    mouse_movement_dejitter_ = true;
    mouse_movement_x_ = 0;
    mouse_movement_y_ = value;
    return;
  }

  last_mouse_movement_time_ = event_time;

  if (mouse_movement_dejitter_)
  {
    mouse_movement_y_ += value;
    SPDLOG_TRACE("mouse movement y {}", mouse_movement_y_);

    if (mouse_movement_y_ > options_.mouse_movement_dejitter_distance)
    {
      mouse_movement_dejitter_ = false;
      value = mouse_movement_y_ - options_.mouse_movement_dejitter_distance;
    }
    else if (mouse_movement_y_ < -options_.mouse_movement_dejitter_distance)
    {
      mouse_movement_dejitter_ = false;
      value = mouse_movement_y_ + options_.mouse_movement_dejitter_distance;
    }
    else
    {
      return;
    }
  }

  delta_ -= delta_decrease_per_mouse_movement_ * std::abs(value);

  if (delta_ < mouse_movement_braking_cut_off_delta_)
  {
    SPDLOG_DEBUG("mouse movement braking stop");
    delta_ = 0;
  }
  else
  {
    SPDLOG_TRACE("mouse movement braking");
  }
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
