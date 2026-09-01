// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Wayne6530

#include "session.h"

#include <algorithm>
#include <cerrno>
#include <chrono>
#include <cstring>
#include <poll.h>
#include <sys/eventfd.h>
#include <sys/select.h>
#include <unistd.h>

#include <spdlog/spdlog.h>

namespace smooth_scroll
{

std::unique_ptr<Session> Session::create(SessionResources resources, const SessionConfig& config,
                                         VirtualDevice& virtual_device, IpcServer& ipc,
                                         const std::atomic_bool& shutdown, int shutdown_fd, CreateResult& result)
{
  auto session = std::unique_ptr<Session>{
    new Session{ std::move(resources), config, virtual_device, ipc, shutdown_fd }
  };
  result = session->initialize(shutdown);
  if (result != CreateResult::Success)
    return nullptr;
  return session;
}

Session::Session(SessionResources resources, const SessionConfig& config, VirtualDevice& virtual_device,
                 IpcServer& ipc, int shutdown_fd)
  : mouse_{ std::move(resources.devices.mouse) }
  , monitor_{ std::move(resources.monitor) }
  , config_{ config }
  , virtual_device_{ virtual_device }
  , ipc_{ ipc }
  , wheel_smoother_{ config.smoother }
  , shutdown_fd_{ shutdown_fd }
{
  keyboards_.reserve(resources.devices.keyboards.size());
  for (auto& device : resources.devices.keyboards)
    keyboards_.push_back({ std::move(device), {}, 0 });

  for (unsigned int key : config.keyboard_braking_keys)
  {
    if (key < KEY_CNT && !isPointerButton(key))
      braking_keys_[key] = true;
  }
  // Entering modifier passthrough must also stop an already-running inertial scroll.
  for (unsigned int key : config.keyboard_passthrough_keys)
  {
    if (key >= KEY_CNT || isPointerButton(key))
      continue;
    braking_keys_[key] = true;
    passthrough_keys_[key] = true;
  }

  for (std::size_t code = 0; code < mouse_.info().capabilities.keys.size(); ++code)
  {
    if (mouse_.info().capabilities.keys[code])
      supported_mouse_keys_.push_back(static_cast<int>(code));
  }
  events_.reserve(16);
}

Session::~Session()
{
  if (grabbed_)
    libevdev_grab(mouse_.evdev(), LIBEVDEV_UNGRAB);
  ipc_.setDisconnected();
}

Session::CreateResult Session::initialize(const std::atomic_bool& shutdown)
{
  SPDLOG_INFO("Input device: {} ({})", mouse_.info().name, mouse_.info().path);
  SPDLOG_INFO("Input device ID: bus {:#x} vendor {:#x} product {:#x}", mouse_.info().bus_type, mouse_.info().vendor_id,
              mouse_.info().product_id);

  while (!shutdown.load(std::memory_order_relaxed))
  {
    CreateResult result = discardPendingMouseEvents();
    if (result != CreateResult::Success)
      return result;
    result = waitUntilAllKeysReleased(shutdown);
    if (result != CreateResult::Success)
      return result;

    const int grab_result = libevdev_grab(mouse_.evdev(), LIBEVDEV_GRAB);
    if (grab_result < 0)
    {
      if (grab_result == -ENODEV)
      {
        SPDLOG_WARN("Mouse device lost before Session grab: {}", mouse_.info().path);
        return CreateResult::MouseLost;
      }
      SPDLOG_ERROR("Failed to grab {}: {}", mouse_.info().path, std::strerror(-grab_result));
      return CreateResult::InputOutputError;
    }
    grabbed_ = true;

    // Events queued before EVIOCGRAB were already visible to the desktop.
    // Drop them, but retry outside the grab if a key crossed the boundary so
    // its physical release can reach the original consumer.
    result = discardPendingMouseEvents();
    if (result != CreateResult::Success)
      return result;
    if (!anyPhysicalKeyPressed())
      break;

    const int ungrab_result = libevdev_grab(mouse_.evdev(), LIBEVDEV_UNGRAB);
    if (ungrab_result < 0)
    {
      if (ungrab_result == -ENODEV)
      {
        SPDLOG_WARN("Mouse device lost while establishing the Session boundary: {}", mouse_.info().path);
        return CreateResult::MouseLost;
      }
      SPDLOG_ERROR("Failed to ungrab {} while establishing the Session boundary: {}", mouse_.info().path,
                   std::strerror(-ungrab_result));
      return CreateResult::InputOutputError;
    }
    grabbed_ = false;
  }

  if (!grabbed_)
    return CreateResult::Shutdown;

  for (KeyboardState& keyboard : keyboards_)
  {
    for (unsigned int key : config_.keyboard_passthrough_keys)
    {
      if (key >= KEY_CNT || isPointerButton(key))
        continue;
      if (libevdev_get_event_value(keyboard.device.evdev(), EV_KEY, key) != 0)
      {
        keyboard.passthrough_pressed[key] = true;
        ++keyboard.num_passthrough;
        ++num_passthrough_;
      }
    }
  }

  ipc_.setConnected();
  ipc_.setPassthrough(num_passthrough_ > 0);
  ipc_.setAutoScrollAxes(wheel_smoother_.auto_scroll_horizontal_enabled(),
                         wheel_smoother_.auto_scroll_vertical_enabled());
  return CreateResult::Success;
}

Session::CreateResult Session::discardPendingMouseEvents()
{
  input_event event{};
  int read_flag = LIBEVDEV_READ_FLAG_NORMAL;
  while (true)
  {
    const int result = libevdev_next_event(mouse_.evdev(), read_flag, &event);
    if (result == LIBEVDEV_READ_STATUS_SYNC)
    {
      read_flag = LIBEVDEV_READ_FLAG_SYNC;
      continue;
    }
    if (result == LIBEVDEV_READ_STATUS_SUCCESS)
      continue;
    if (result == -EAGAIN)
      return CreateResult::Success;
    if (result == -ENODEV)
    {
      SPDLOG_WARN("Mouse device lost before Session start: {}", mouse_.info().path);
      return CreateResult::MouseLost;
    }
    SPDLOG_ERROR("Failed to read mouse {} before Session start: {}", mouse_.info().path, std::strerror(-result));
    return CreateResult::InputOutputError;
  }
}

Session::CreateResult Session::waitUntilAllKeysReleased(const std::atomic_bool& shutdown)
{
  while (!shutdown.load(std::memory_order_relaxed) && anyPhysicalKeyPressed())
  {
    pollfd descriptors[]{ { mouse_.fd(), POLLIN, 0 }, { shutdown_fd_, POLLIN, 0 } };
    const int poll_result = poll(descriptors, 2, -1);
    if (poll_result < 0)
    {
      if (errno == EINTR)
        continue;
      SPDLOG_ERROR("Failed to wait for mouse keys to be released: {}", std::strerror(errno));
      return CreateResult::InputOutputError;
    }
    if (descriptors[1].revents & POLLIN)
      return CreateResult::Shutdown;

    if (descriptors[0].revents & (POLLIN | POLLERR | POLLHUP | POLLNVAL))
    {
      const CreateResult result = discardPendingMouseEvents();
      if (result != CreateResult::Success)
        return result;
    }
  }
  return shutdown.load(std::memory_order_relaxed) ? CreateResult::Shutdown : CreateResult::Success;
}

bool Session::anyPhysicalKeyPressed() const noexcept
{
  return std::any_of(supported_mouse_keys_.begin(), supported_mouse_keys_.end(),
                     [&](int key) { return libevdev_get_event_value(mouse_.evdev(), EV_KEY, key) != 0; });
}

bool Session::writeEvents(const timeval& time)
{
  if (virtual_device_.writeFrame(events_, time))
    return true;
  SPDLOG_ERROR("Failed to write uinput events: {}", std::strerror(errno));
  return false;
}

void Session::handleObservedKey(unsigned int code, int value, std::array<bool, KEY_CNT>& passthrough_pressed,
                                int& source_num_passthrough)
{
  if (code >= KEY_CNT || isPointerButton(code) || value == 2)
    return;

  if (braking_keys_[code])
    stopMotion();

  if (!passthrough_keys_[code])
    return;

  if (value == 1 && !passthrough_pressed[code])
  {
    passthrough_pressed[code] = true;
    ++source_num_passthrough;
    ++num_passthrough_;
  }
  else if (value == 0 && passthrough_pressed[code])
  {
    passthrough_pressed[code] = false;
    --source_num_passthrough;
    --num_passthrough_;
  }
  ipc_.setPassthrough(num_passthrough_ > 0);
}

void Session::stopMotion()
{
  wheel_smoother_.stop();
  ipc_.setSpeed(0, false, false);
  updateAutoScrollIpc();
}

void Session::updateAutoScrollIpc()
{
  ipc_.setAutoScroll(wheel_smoother_.auto_scroll());
  ipc_.setAutoScrollOffset(wheel_smoother_.auto_scroll_offset_x(), wheel_smoother_.auto_scroll_offset_y());
}

Session::RunResult Session::run(const std::atomic_bool& shutdown)
{
  input_event event{};
  fd_set descriptors;
  int max_fd = -1;
  const int stop_fd = monitor_->stopFd();
  const auto rebuildDescriptors = [&] {
    FD_ZERO(&descriptors);
    FD_SET(mouse_.fd(), &descriptors);
    FD_SET(stop_fd, &descriptors);
    FD_SET(shutdown_fd_, &descriptors);
    max_fd = std::max({ mouse_.fd(), stop_fd, shutdown_fd_ });
    for (const KeyboardState& keyboard : keyboards_)
    {
      FD_SET(keyboard.device.fd(), &descriptors);
      max_fd = std::max(max_fd, keyboard.device.fd());
    }
  };
  rebuildDescriptors();
  bool restart_pending = false;

  while (!shutdown.load(std::memory_order_relaxed))
  {
    fd_set read_fds = descriptors;

    auto timeout = wheel_smoother_.timeout();
    const int select_result = select(max_fd + 1, &read_fds, nullptr, nullptr, timeout ? &*timeout : nullptr);
    if (select_result < 0)
    {
      if (errno == EINTR)
        continue;
      SPDLOG_ERROR("select failed: {}", std::strerror(errno));
      return RunResult::InputOutputError;
    }
    if (select_result == 0)
    {
      if (ipc_.checkBrakeRequest())
      {
        stopMotion();
      }
      else
      {
        const auto tick_result = wheel_smoother_.tick();
        for (std::size_t i = 0; i < tick_result.count; ++i)
          events_.push_back(tick_result.events[i]);
        if (tick_result.count > 0 && !writeEvents(tick_result.events[0].time))
          return RunResult::InputOutputError;
        ipc_.setSpeed(wheel_smoother_.speed(), wheel_smoother_.positive(), wheel_smoother_.horizontal());
      }
      continue;
    }

    if (FD_ISSET(shutdown_fd_, &read_fds))
      return RunResult::Shutdown;

    if (FD_ISSET(stop_fd, &read_fds))
    {
      eventfd_t value;
      while (eventfd_read(stop_fd, &value) == 0)
      {
      }
      restart_pending = true;
    }

    std::optional<timeval> last_input_event_time;

    for (auto iterator = keyboards_.begin(); iterator != keyboards_.end();)
    {
      if (!FD_ISSET(iterator->device.fd(), &read_fds))
      {
        ++iterator;
        continue;
      }

      int result = 0;
      int read_flag = LIBEVDEV_READ_FLAG_NORMAL;
      while (true)
      {
        result = libevdev_next_event(iterator->device.evdev(), read_flag, &event);
        if (result == LIBEVDEV_READ_STATUS_SYNC)
        {
          if (event.type == EV_SYN && event.code == SYN_DROPPED)
            read_flag = LIBEVDEV_READ_FLAG_SYNC;
        }
        else if (result != LIBEVDEV_READ_STATUS_SUCCESS)
        {
          break;
        }

        last_input_event_time = event.time;
        if (event.type == EV_KEY)
          handleObservedKey(event.code, event.value, iterator->passthrough_pressed, iterator->num_passthrough);
      }

      if (result == -ENODEV)
      {
        SPDLOG_WARN("Keyboard device lost: {}", iterator->device.info().path);
        num_passthrough_ -= iterator->num_passthrough;
        ipc_.setPassthrough(num_passthrough_ > 0);
        iterator = keyboards_.erase(iterator);
        rebuildDescriptors();
      }
      else if (result < 0 && result != -EAGAIN)
      {
        SPDLOG_ERROR("Failed to read keyboard {}: {}", iterator->device.info().path, std::strerror(-result));
        return RunResult::InputOutputError;
      }
      else
      {
        ++iterator;
      }
    }

    if (FD_ISSET(mouse_.fd(), &read_fds))
    {
      int result = 0;
      int read_flag = LIBEVDEV_READ_FLAG_NORMAL;
      while (true)
      {
        result = libevdev_next_event(mouse_.evdev(), read_flag, &event);
        if (result == LIBEVDEV_READ_STATUS_SYNC)
        {
          if (event.type == EV_SYN && event.code == SYN_DROPPED)
          {
            events_.clear();
            wheel_smoother_.hardReset();
            ipc_.resetMotionState();
            read_flag = LIBEVDEV_READ_FLAG_SYNC;
            continue;
          }
          if (event.type == EV_SYN && event.code == SYN_REPORT)
          {
            if (!writeEvents(event.time))
              return RunResult::InputOutputError;
          }
          else if (event.type != EV_MSC)
          {
            if (event.type == EV_KEY)
              handleObservedKey(event.code, event.value, mouse_passthrough_pressed_, mouse_num_passthrough_);
            events_.push_back(event);
          }
          last_input_event_time = event.time;
          continue;
        }
        if (result != LIBEVDEV_READ_STATUS_SUCCESS)
          break;

        last_input_event_time = event.time;
        switch (event.type)
        {
          case EV_REL:
            switch (event.code)
            {
              case REL_WHEEL:
              case REL_HWHEEL:
                if (num_passthrough_ || ipc_.isForcePassthroughEnabled())
                {
                  events_.push_back(event);
                }
                else
                {
                  if (ipc_.checkBrakeRequest())
                    wheel_smoother_.stop();
                  if (auto wheel_event =
                          wheel_smoother_.handleEvent(event.time, event.value > 0, event.code == REL_HWHEEL))
                    events_.push_back(*wheel_event);
                  ipc_.setSpeed(wheel_smoother_.speed(), wheel_smoother_.positive(), wheel_smoother_.horizontal());
                  updateAutoScrollIpc();
                }
                break;
              case REL_WHEEL_HI_RES:
              case REL_HWHEEL_HI_RES:
                if (num_passthrough_ || ipc_.isForcePassthroughEnabled())
                  events_.push_back(event);
                break;
              case REL_X:
                if (wheel_smoother_.handleRelXEvent(event))
                  events_.push_back(event);
                break;
              case REL_Y:
                if (wheel_smoother_.handleRelYEvent(event))
                  events_.push_back(event);
                break;
              default:
                events_.push_back(event);
                break;
            }
            break;

          case EV_KEY: {
            handleObservedKey(event.code, event.value, mouse_passthrough_pressed_, mouse_num_passthrough_);
            // Composite mice can expose keyboard or consumer keys on the same
            // grabbed event node. Forward those keys, but do not reinterpret
            // them as pointer buttons that can control mouse-only modes.
            if (!isPointerButton(event.code))
            {
              events_.push_back(event);
              break;
            }
            if (event.code == config_.auto_scroll_button)
            {
              const auto button_result = wheel_smoother_.handleAutoScrollButton(event.time, event.code, event.value);
              if (button_result == WheelSmoother::ButtonResult::ReplayClick)
              {
                input_event press_event = event;
                press_event.value = 1;
                events_.push_back(press_event);
                events_.push_back({ event.time, EV_SYN, SYN_REPORT, 0 });
                events_.push_back(event);
              }
              else if (button_result == WheelSmoother::ButtonResult::Passthrough)
              {
                events_.push_back(event);
              }
              ipc_.setSpeed(0, false, false);
              updateAutoScrollIpc();
            }
            else if (event.code == config_.drag_view_button)
            {
              const auto button_result = wheel_smoother_.handleDragViewButton(event.time, event.value);
              if (button_result == WheelSmoother::ButtonResult::ReplayClick)
              {
                input_event press_event = event;
                press_event.value = 1;
                events_.push_back(press_event);
                events_.push_back({ event.time, EV_SYN, SYN_REPORT, 0 });
                events_.push_back(event);
              }
              else if (button_result == WheelSmoother::ButtonResult::Passthrough)
              {
                events_.push_back(event);
              }
              ipc_.setSpeed(0, false, false);
              updateAutoScrollIpc();
              ipc_.setDragView(wheel_smoother_.drag_view());
            }
            else if (event.code == config_.free_spin_button)
            {
              if (!wheel_smoother_.handleFreeSpinButton(event.value))
                events_.push_back(event);
              ipc_.setFreeSpin(wheel_smoother_.free_spin());
            }
            else
            {
              const auto button_result = wheel_smoother_.handleOrdinaryButton(event.code, event.value);
              if (button_result == WheelSmoother::ButtonResult::Passthrough)
                events_.push_back(event);
              ipc_.setSpeed(0, false, false);
              updateAutoScrollIpc();
            }
            break;
          }

          case EV_MSC:
            break;

          case EV_SYN:
            if (event.code == SYN_REPORT)
            {
              switch (wheel_smoother_.handleReportEvent(event.time))
              {
                case WheelSmoother::ReportResult::ScrollStopped:
                  ipc_.setSpeed(0, false, false);
                  break;
                case WheelSmoother::ReportResult::AutoScrollOffsetChanged:
                  ipc_.setAutoScrollOffset(wheel_smoother_.auto_scroll_offset_x(),
                                           wheel_smoother_.auto_scroll_offset_y());
                  break;
                case WheelSmoother::ReportResult::None:
                  break;
              }
              if (!writeEvents(event.time))
                return RunResult::InputOutputError;
            }
            break;

          default:
            events_.push_back(event);
            break;
        }
      }

      if (result == -ENODEV)
      {
        SPDLOG_WARN("Mouse device lost: {}", mouse_.info().path);
        return RunResult::MouseLost;
      }
      if (result < 0 && result != -EAGAIN)
      {
        SPDLOG_ERROR("Failed to read mouse {}: {}", mouse_.info().path, std::strerror(-result));
        return RunResult::InputOutputError;
      }
    }

    if (last_input_event_time)
    {
      if (auto next_tick_time = wheel_smoother_.next_tick_time())
      {
        const std::chrono::microseconds event_time = std::chrono::seconds{ last_input_event_time->tv_sec } +
                                                     std::chrono::microseconds{ last_input_event_time->tv_usec };
        if (event_time > *next_tick_time)
        {
          if (ipc_.checkBrakeRequest())
          {
            stopMotion();
          }
          else
          {
            const auto tick_result = wheel_smoother_.tick();
            for (std::size_t i = 0; i < tick_result.count; ++i)
              events_.push_back(tick_result.events[i]);
            if (tick_result.count > 0 && !writeEvents(tick_result.events[0].time))
              return RunResult::InputOutputError;
            ipc_.setSpeed(wheel_smoother_.speed(), wheel_smoother_.positive(), wheel_smoother_.horizontal());
          }
        }
      }
    }

    if (restart_pending && !anyPhysicalKeyPressed() && events_.empty())
    {
      stopMotion();
      return RunResult::RestartRequested;
    }
  }

  return RunResult::Shutdown;
}

}  // namespace smooth_scroll
