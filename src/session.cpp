// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Wayne6530

#include "session.h"

#include <algorithm>
#include <cerrno>
#include <chrono>
#include <cstring>
#include <poll.h>
#include <sys/eventfd.h>
#include <unistd.h>

#include <spdlog/spdlog.h>

namespace smooth_scroll
{

std::unique_ptr<Session> Session::create(SessionResources resources, ProfileSet& profile_set,
                                         VirtualDevice& virtual_device, IpcServer& ipc,
                                         const std::atomic_bool& shutdown, int shutdown_fd, CreateResult& result)
{
  auto session =
      std::unique_ptr<Session>{ new Session{ std::move(resources), profile_set, virtual_device, ipc, shutdown_fd } };
  result = session->initialize(shutdown);
  if (result != CreateResult::Success)
    return nullptr;
  return session;
}

Session::Session(SessionResources resources, ProfileSet& profile_set, VirtualDevice& virtual_device, IpcServer& ipc,
                 int shutdown_fd)
  : mouse_{ std::move(resources.devices.mouse) }
  , monitor_{ std::move(resources.monitor) }
  , virtual_device_{ virtual_device }
  , ipc_{ ipc }
  , profile_set_{ profile_set }
  , shutdown_fd_{ shutdown_fd }
  , direct_mode_toggle_key_{ resources.direct_mode_toggle_key }
{
  keyboards_.reserve(resources.devices.keyboards.size());
  for (auto& device : resources.devices.keyboards)
    keyboards_.push_back({ std::move(device), {} });

  for (std::size_t code = 0; code < mouse_.info().capabilities.keys.size(); ++code)
  {
    if (mouse_.info().capabilities.keys[code])
      supported_mouse_keys_.push_back(static_cast<int>(code));
  }
  events_.reserve(16);
}

Session::~Session()
{
  // Device loss may prevent special-button releases from ever arriving.
  smoother().hardReset();
  if (grabbed_)
    libevdev_grab(mouse_.evdev(), LIBEVDEV_UNGRAB);
  ipc_.setDisconnected();
}

Session::CreateResult Session::initialize(const std::atomic_bool& shutdown)
{
  SPDLOG_INFO("Input device: {} ({})", mouse_.info().name, mouse_.info().path);
  SPDLOG_INFO("Input device ID: bus {:#x} vendor {:#x} product {:#x}", mouse_.info().bus_type, mouse_.info().vendor_id,
              mouse_.info().product_id);

  if (direct_mode_toggle_key_ &&
      std::none_of(keyboards_.begin(), keyboards_.end(), [&](const KeyboardState& keyboard) {
        return keyboard.device.info().capabilities.keys[direct_mode_toggle_key_];
      }))
    SPDLOG_WARN("No other input node supports direct_mode_toggle_key {}", direct_mode_toggle_key_);

  if (ipc_.directMode())
  {
    // The active-mouse detector may have consumed only part of a queued frame.
    const auto status = discardPendingMouseEvents();
    if (status != CreateResult::Success)
      return status;
  }
  else
  {
    while (!grabbed_)
    {
      auto result = discardPendingMouseEvents();
      if (result == CreateResult::Success)
        result = waitUntilAllKeysReleased(shutdown);
      if (result == CreateResult::Success)
        result = tryGrabMouse();
      if (result != CreateResult::Success)
        return result;
    }
  }

  for (KeyboardState& keyboard : keyboards_)
  {
    for (unsigned int key = 0; key < KEY_CNT; ++key)
    {
      if (profile_set_.passthrough_union_keys[key] &&
          libevdev_get_event_value(keyboard.device.evdev(), EV_KEY, key) != 0)
        handleObservedKey(key, 1, keyboard.passthrough);
    }
  }

  if (grabbed_)
    ipc_.setConnected();
  else
    ipc_.setDirectModeConnected();
  publishProfileState();
  return CreateResult::Success;
}

// A press crossing EVIOCGRAB must still release through the original device.
Session::CreateResult Session::tryGrabMouse()
{
  int result = libevdev_grab(mouse_.evdev(), LIBEVDEV_GRAB);
  if (!result)
  {
    grabbed_ = true;
    auto status = discardPendingMouseEvents();
    if (status != CreateResult::Success || !anyPhysicalKeyPressed())
      return status;
    result = libevdev_grab(mouse_.evdev(), LIBEVDEV_UNGRAB);
    if (!result)
    {
      grabbed_ = false;
      return CreateResult::Success;
    }
  }
  if (result == -EBUSY && !grabbed_)
  {
    SPDLOG_WARN("Mouse is grabbed by another client");
    return CreateResult::MouseBusy;
  }
  SPDLOG_ERROR("Mouse grab transition failed: {}", std::strerror(-result));
  return result == -ENODEV ? CreateResult::MouseLost : CreateResult::InputOutputError;
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
      SPDLOG_WARN("Device lost at input boundary: {}", mouse_.info().path);
      return CreateResult::MouseLost;
    }
    SPDLOG_ERROR("Failed to drain {} at input boundary: {}", mouse_.info().path, std::strerror(-result));
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

void Session::handleObservedKey(unsigned int code, int value, KeyboardPassthroughState& source)
{
  if (code >= KEY_CNT || isPointerButton(code) || value == 2)
    return;
  if (activeProfile().braking_keys[code])
    stopMotion();
  if (!profile_set_.passthrough_union_keys[code])
    return;
  const bool active = activeProfile().passthrough_keys[code];
  const int change = source.update(code, value, active);
  num_union_pressed_ += change;
  if (active)
    num_passthrough_ += change;
  ipc_.setKeyboardPassthrough(num_passthrough_ > 0);
}

void Session::stopMotion()
{
  smoother().stop();
  ipc_.setSpeed(0, false, false);
  updateAutoScrollIpc();
}

bool Session::processIpcRequests()
{
  const bool braked = ipc_.checkBrakeRequest();
  if (braked)
    stopMotion();

  // Braking can end a latched mode and make a pending profile switch eligible.
  const auto requested = ipc_.requestedProfileId();
  if (requested >= profile_set_.profiles.size() || requested == active_profile_index_ ||
      !activeProfile().canDeactivate(num_union_pressed_))
    return braked;
  smoother().stop();
  active_profile_index_ = requested;
  publishProfileState();
  return true;
}

void Session::publishProfileState()
{
  ipc_.resetMotionState();
  ipc_.setKeyboardPassthrough(num_passthrough_ > 0);
  ipc_.setAutoScrollAxes(smoother().auto_scroll_horizontal_enabled(), smoother().auto_scroll_vertical_enabled());
  ipc_.setActiveProfileId(active_profile_index_);
}

void Session::updateAutoScrollIpc()
{
  ipc_.setAutoScroll(smoother().auto_scroll());
  ipc_.setAutoScrollOffset(smoother().auto_scroll_offset_x(), smoother().auto_scroll_offset_y());
}

std::optional<Session::RunResult> Session::readGrabbedMouseEvents(std::optional<timeval>& last_input_event_time)
{
  input_event event{};
  int result;
  int read_flag = LIBEVDEV_READ_FLAG_NORMAL;
  while (true)
  {
    result = libevdev_next_event(mouse_.evdev(), read_flag, &event);
    if (result == LIBEVDEV_READ_STATUS_SYNC)
    {
      read_flag = LIBEVDEV_READ_FLAG_SYNC;
      if (event.type == EV_SYN && event.code == SYN_DROPPED)
      {
        events_.clear();
        mouse_frame_ = {};
        smoother().hardReset();
        ipc_.resetMotionState();
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
          handleObservedKey(event.code, event.value, mouse_passthrough_);
        events_.push_back(event);
      }
      last_input_event_time = event.time;
      continue;
    }
    if (result != LIBEVDEV_READ_STATUS_SUCCESS)
      break;

    last_input_event_time = event.time;
    processIpcRequests();
    const auto& profile = activeProfile().config;
    const bool wheel_passthrough = profile.wheel_passthrough;
    switch (event.type)
    {
      case EV_REL:
        switch (event.code)
        {
          case REL_WHEEL:
          case REL_HWHEEL:
            if (num_passthrough_ || wheel_passthrough)
            {
              events_.push_back(event);
            }
            else
            {
              if (auto wheel_event = smoother().handleEvent(event.time, event.value > 0, event.code == REL_HWHEEL))
                events_.push_back(*wheel_event);
              ipc_.setSpeed(smoother().speed(), smoother().positive(), smoother().horizontal());
              updateAutoScrollIpc();
            }
            break;
          case REL_WHEEL_HI_RES:
          case REL_HWHEEL_HI_RES:
            if (num_passthrough_ || wheel_passthrough)
              events_.push_back(event);
            break;
          case REL_X:
            if (smoother().handleRelXEvent(event, mouse_frame_))
              events_.push_back(event);
            break;
          case REL_Y:
            if (smoother().handleRelYEvent(event, mouse_frame_))
              events_.push_back(event);
            break;
          default:
            events_.push_back(event);
            break;
        }
        break;

      case EV_KEY: {
        handleObservedKey(event.code, event.value, mouse_passthrough_);
        // Composite mice can expose keyboard or consumer keys on the same
        // grabbed event node. Forward those keys, but do not reinterpret
        // them as pointer buttons that can control mouse-only modes.
        if (!isPointerButton(event.code))
        {
          events_.push_back(event);
          break;
        }
        if (event.code == profile.auto_scroll_button)
        {
          const auto button_result = smoother().handleAutoScrollButton(event.time, event.code, event.value);
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
        else if (event.code == profile.drag_view_button)
        {
          const auto button_result = smoother().handleDragViewButton(event.time, event.value);
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
          ipc_.setDragView(smoother().drag_view());
        }
        else if (event.code == profile.free_spin_button)
        {
          if (!smoother().handleFreeSpinButton(event.value))
            events_.push_back(event);
          ipc_.setFreeSpin(smoother().free_spin());
        }
        else
        {
          const auto button_result = smoother().handleOrdinaryButton(event.code, event.value);
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
          switch (smoother().handleReportEvent(event.time, mouse_frame_))
          {
            case WheelSmoother::ReportResult::ScrollStopped:
              ipc_.setSpeed(0, false, false);
              break;
            case WheelSmoother::ReportResult::AutoScrollOffsetChanged:
              ipc_.setAutoScrollOffset(smoother().auto_scroll_offset_x(), smoother().auto_scroll_offset_y());
              break;
            case WheelSmoother::ReportResult::None:
              break;
          }
          mouse_frame_ = {};
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
  return std::nullopt;
}

std::optional<Session::RunResult> Session::tryToggleMouseMode()
{
  if (grabbed_)
  {
    if (!events_.empty() || anyPhysicalKeyPressed())
      return std::nullopt;

    const int result = libevdev_grab(mouse_.evdev(), LIBEVDEV_UNGRAB);
    if (result < 0)
      return result == -ENODEV ? RunResult::MouseLost : RunResult::InputOutputError;
    grabbed_ = false;
    smoother().hardReset();
    ipc_.resetMotionState();
    ipc_.setDirectModeConnected();
  }
  else
  {
    // Direct-mode events have already reached applications. Drain them to refresh the cached key state.
    auto result = discardPendingMouseEvents();
    if (result == CreateResult::Success && anyPhysicalKeyPressed())
      return std::nullopt;
    if (result == CreateResult::Success)
      result = tryGrabMouse();
    if (result == CreateResult::MouseBusy)
      return std::nullopt;
    if (result != CreateResult::Success)
      return result == CreateResult::MouseLost ? RunResult::MouseLost : RunResult::InputOutputError;
    if (!grabbed_)
      return std::nullopt;
    ipc_.setConnected();
  }
  processIpcRequests();
  return std::nullopt;
}

Session::RunResult Session::run(const std::atomic_bool& shutdown)
{
  input_event event{};
  const int stop_fd = monitor_ ? monitor_->stopFd() : -1;
  std::vector<pollfd> descriptors{ { mouse_.fd(), POLLIN, 0 }, { stop_fd, POLLIN, 0 }, { shutdown_fd_, POLLIN, 0 } };
  for (const auto& keyboard : keyboards_)
    descriptors.push_back({ keyboard.device.fd(), POLLIN, 0 });
  bool restart_pending = false;
  while (!shutdown.load(std::memory_order_relaxed))
  {
    // Keep the idle physical fd for hangups, without waking on every movement.
    descriptors[0].events = grabbed_ ? POLLIN : 0;

    processIpcRequests();
    auto timeout = smoother().timeout();
    timespec interval{};
    if (timeout)
      interval = { timeout->tv_sec, timeout->tv_usec * 1000 };
    const int poll_result = ppoll(descriptors.data(), descriptors.size(), timeout ? &interval : nullptr, nullptr);
    if (poll_result < 0)
    {
      if (errno == EINTR)
        continue;
      SPDLOG_ERROR("ppoll failed: {}", std::strerror(errno));
      return RunResult::InputOutputError;
    }
    if (poll_result == 0)
    {
      if (!processIpcRequests())
      {
        const auto tick_result = smoother().tick();
        for (std::size_t i = 0; i < tick_result.count; ++i)
          events_.push_back(tick_result.events[i]);
        if (tick_result.count > 0 && !writeEvents(tick_result.events[0].time))
          return RunResult::InputOutputError;
        ipc_.setSpeed(smoother().speed(), smoother().positive(), smoother().horizontal());
      }
      continue;
    }

    if (descriptors[2].revents)
      return RunResult::Shutdown;

    if (descriptors[1].revents)
    {
      eventfd_t value;
      while (eventfd_read(stop_fd, &value) == 0)
      {
      }
      restart_pending = true;
    }

    std::optional<timeval> last_input_event_time;

    for (size_t index = 0; index < keyboards_.size();)
    {
      const size_t poll_index = 3 + index;  // Mouse, monitor and shutdown precede the keyboards.
      if (!descriptors[poll_index].revents)
      {
        ++index;
        continue;
      }

      auto& keyboard = keyboards_[index];
      int result;
      int read_flag = LIBEVDEV_READ_FLAG_NORMAL;
      while (true)
      {
        result = libevdev_next_event(keyboard.device.evdev(), read_flag, &event);
        if (result == LIBEVDEV_READ_STATUS_SYNC)
          read_flag = LIBEVDEV_READ_FLAG_SYNC;
        else if (result == LIBEVDEV_READ_STATUS_SUCCESS)
          processIpcRequests();
        else
          break;

        last_input_event_time = event.time;
        if (event.type == EV_KEY)
        {
          if (result == LIBEVDEV_READ_STATUS_SUCCESS && direct_mode_toggle_key_ != 0 &&
              event.code == direct_mode_toggle_key_ && event.value == 1)
          {
            if (auto error = tryToggleMouseMode())
              return *error;
          }
          handleObservedKey(event.code, event.value, keyboard.passthrough);
        }
      }

      if (result == -ENODEV)
      {
        SPDLOG_WARN("Keyboard device lost: {}", keyboard.device.info().path);
        num_passthrough_ -= keyboard.passthrough.active_pressed;
        num_union_pressed_ -= keyboard.passthrough.union_pressed;
        ipc_.setKeyboardPassthrough(num_passthrough_ > 0);
        keyboards_.erase(keyboards_.begin() + index);
        descriptors.erase(descriptors.begin() + poll_index);
        continue;
      }
      if (result < 0 && result != -EAGAIN)
      {
        SPDLOG_ERROR("Failed to read keyboard {}: {}", keyboard.device.info().path, std::strerror(-result));
        return RunResult::InputOutputError;
      }
      ++index;
    }

    if (descriptors[0].revents & (POLLERR | POLLHUP | POLLNVAL))
      return RunResult::MouseLost;
    if (grabbed_ && (descriptors[0].revents & POLLIN))
    {
      if (auto error = readGrabbedMouseEvents(last_input_event_time))
        return *error;
    }

    if (last_input_event_time)
    {
      if (auto next_tick_time = smoother().next_tick_time())
      {
        const std::chrono::microseconds event_time = std::chrono::seconds{ last_input_event_time->tv_sec } +
                                                     std::chrono::microseconds{ last_input_event_time->tv_usec };
        if (event_time > *next_tick_time)
        {
          if (!processIpcRequests())
          {
            const auto tick_result = smoother().tick();
            for (std::size_t i = 0; i < tick_result.count; ++i)
              events_.push_back(tick_result.events[i]);
            if (tick_result.count > 0 && !writeEvents(tick_result.events[0].time))
              return RunResult::InputOutputError;
            ipc_.setSpeed(smoother().speed(), smoother().positive(), smoother().horizontal());
          }
        }
      }
    }

    if (restart_pending && (!grabbed_ || (!anyPhysicalKeyPressed() && events_.empty())))
    {
      // Teardown resets motion; the next Session rebuilds device-local input state.
      return RunResult::RestartRequested;
    }
  }

  return RunResult::Shutdown;
}

}  // namespace smooth_scroll
