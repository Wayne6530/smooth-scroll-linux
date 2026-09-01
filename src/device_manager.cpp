// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Wayne6530

#include "device_manager.h"

#include <algorithm>
#include <cerrno>
#include <chrono>
#include <csignal>
#include <cstring>
#include <dirent.h>
#include <limits>
#include <poll.h>
#include <pthread.h>
#include <sys/eventfd.h>
#include <sys/resource.h>
#include <system_error>
#include <unistd.h>
#include <utility>

#include <libudev.h>
#include <spdlog/spdlog.h>

namespace smooth_scroll
{

class UdevInputSource
{
public:
  enum class ReceiveResult
  {
    Empty,
    Irrelevant,
    RelevantAdd,
    Overflow,
    Error,
  };

  [[nodiscard]] static std::unique_ptr<UdevInputSource> create()
  {
    udev* context = udev_new();
    if (!context)
    {
      SPDLOG_ERROR("Failed to create udev context");
      return nullptr;
    }

    udev_monitor* monitor = udev_monitor_new_from_netlink(context, "udev");
    if (!monitor || udev_monitor_filter_add_match_subsystem_devtype(monitor, "input", nullptr) < 0 ||
        udev_monitor_enable_receiving(monitor) < 0)
    {
      SPDLOG_ERROR("Failed to start udev input monitor");
      if (monitor)
        udev_monitor_unref(monitor);
      udev_unref(context);
      return nullptr;
    }

    if (udev_monitor_get_fd(monitor) < 0)
    {
      SPDLOG_ERROR("Failed to obtain the udev input monitor file descriptor");
      udev_monitor_unref(monitor);
      udev_unref(context);
      return nullptr;
    }

    return std::unique_ptr<UdevInputSource>{ new UdevInputSource{ context, monitor } };
  }

  ~UdevInputSource()
  {
    if (monitor_)
      udev_monitor_unref(monitor_);
    if (context_)
      udev_unref(context_);
  }

  UdevInputSource(const UdevInputSource&) = delete;
  UdevInputSource& operator=(const UdevInputSource&) = delete;

  [[nodiscard]] int fd() const noexcept
  {
    return udev_monitor_get_fd(monitor_);
  }

  [[nodiscard]] ReceiveResult receive(const DeviceConfig& config,
                                      const std::vector<unsigned int>& relevant_keyboard_keys)
  {
    errno = 0;
    udev_device* device = udev_monitor_receive_device(monitor_);
    if (!device)
    {
      if (errno == 0 || errno == EAGAIN || errno == EWOULDBLOCK)
        return ReceiveResult::Empty;
      if (errno == ENOBUFS)
      {
        SPDLOG_WARN("udev input monitor overflowed; rebuilding the device snapshot");
        return ReceiveResult::Overflow;
      }
      SPDLOG_ERROR("Failed to receive a udev input event: {}", std::strerror(errno));
      return ReceiveResult::Error;
    }

    const auto value_or_empty = [](const char* value) { return value ? std::string{ value } : std::string{}; };
    const std::string action = value_or_empty(udev_device_get_action(device));
    const std::string path = value_or_empty(udev_device_get_devnode(device));
    udev_device_unref(device);

    constexpr const char* event_device_prefix = "/dev/input/event";
    if (action != "add" || path.compare(0, std::strlen(event_device_prefix), event_device_prefix) != 0)
      return ReceiveResult::Irrelevant;

    auto handle = openInputDevice(path, false);
    if (!handle)
    {
      // A post-rule udev event should normally be openable immediately. Conservatively rebuild the snapshot if it
      // is not, so a short-lived permission or device-node race cannot leave acquisition asleep indefinitely.
      return ReceiveResult::RelevantAdd;
    }
    if (handle->info().isVirtualDevice() || isIgnoredDevice(handle->info(), config.ignored_devices))
      return ReceiveResult::Irrelevant;

    const DeviceCapabilities& capabilities = handle->info().capabilities;
    if (capabilities.isMouse() ||
        (!relevant_keyboard_keys.empty() && capabilities.supportsAnyKey(relevant_keyboard_keys)))
      return ReceiveResult::RelevantAdd;
    return ReceiveResult::Irrelevant;
  }

private:
  UdevInputSource(udev* context, udev_monitor* monitor) : context_{ context }, monitor_{ monitor }
  {
  }

  udev* context_;
  udev_monitor* monitor_;
};

namespace
{

using Clock = std::chrono::steady_clock;

enum class DrainResult
{
  Unchanged,
  Changed,
  Error,
};

enum class DebounceResult
{
  Settled,
  Shutdown,
  Error,
};

DrainResult drainUdevEvents(UdevInputSource& source, const DeviceConfig& config,
                            const std::vector<unsigned int>& relevant_keyboard_keys)
{
  DrainResult result = DrainResult::Unchanged;
  while (true)
  {
    switch (source.receive(config, relevant_keyboard_keys))
    {
      case UdevInputSource::ReceiveResult::Empty:
        return result;
      case UdevInputSource::ReceiveResult::Irrelevant:
        break;
      case UdevInputSource::ReceiveResult::RelevantAdd:
      case UdevInputSource::ReceiveResult::Overflow:
        result = DrainResult::Changed;
        break;
      case UdevInputSource::ReceiveResult::Error:
        return DrainResult::Error;
    }
  }
}

int timeoutUntil(Clock::time_point deadline)
{
  const auto remaining = deadline - Clock::now();
  if (remaining <= Clock::duration::zero())
    return 0;

  const auto milliseconds = std::chrono::duration_cast<std::chrono::milliseconds>(remaining).count();
  return static_cast<int>(std::min<int64_t>(std::numeric_limits<int>::max(), std::max<int64_t>(1, milliseconds + 1)));
}

}  // namespace

DeviceMonitor::DeviceMonitor(std::unique_ptr<UdevInputSource> source, DeviceConfig config,
                             std::vector<unsigned int> relevant_keyboard_keys)
  : source_{ std::move(source) }
  , config_{ std::move(config) }
  , relevant_keyboard_keys_{ std::move(relevant_keyboard_keys) }
{
}

DeviceMonitor::~DeviceMonitor()
{
  if (worker_stop_fd_ >= 0)
    eventfd_write(worker_stop_fd_, 1);
  if (worker_.joinable())
    worker_.join();
  if (worker_stop_fd_ >= 0)
    close(worker_stop_fd_);
  if (stop_fd_ >= 0)
    close(stop_fd_);
}

std::unique_ptr<DeviceMonitor> DeviceMonitor::start(std::unique_ptr<UdevInputSource> source, DeviceConfig config,
                                                    std::vector<unsigned int> relevant_keyboard_keys)
{
  auto monitor = std::unique_ptr<DeviceMonitor>{
    new DeviceMonitor{ std::move(source), std::move(config), std::move(relevant_keyboard_keys) }
  };
  if (!monitor->initialize())
    return nullptr;
  return monitor;
}

bool DeviceMonitor::initialize()
{
  stop_fd_ = eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC);
  worker_stop_fd_ = eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC);
  if (stop_fd_ < 0 || worker_stop_fd_ < 0)
  {
    SPDLOG_ERROR("Failed to create DeviceMonitor eventfd: {}", std::strerror(errno));
    return false;
  }

  // Process-directed shutdown signals may otherwise be delivered to this worker, leaving the main thread blocked in
  // the Session input loop. The worker inherits this temporary mask; the creating thread immediately restores its own.
  sigset_t shutdown_signals;
  sigemptyset(&shutdown_signals);
  sigaddset(&shutdown_signals, SIGINT);
  sigaddset(&shutdown_signals, SIGTERM);
  sigset_t previous_signal_mask;
  int result = pthread_sigmask(SIG_BLOCK, &shutdown_signals, &previous_signal_mask);
  if (result != 0)
  {
    SPDLOG_ERROR("Failed to block shutdown signals while starting DeviceMonitor: {}", std::strerror(result));
    return false;
  }

  try
  {
    worker_ = std::thread{ &DeviceMonitor::monitorLoop, this };
  }
  catch (const std::system_error& error)
  {
    pthread_sigmask(SIG_SETMASK, &previous_signal_mask, nullptr);
    SPDLOG_ERROR("Failed to start DeviceMonitor worker: {}", error.what());
    return false;
  }

  result = pthread_sigmask(SIG_SETMASK, &previous_signal_mask, nullptr);
  if (result != 0)
  {
    SPDLOG_ERROR("Failed to restore the main thread signal mask: {}", std::strerror(result));
    eventfd_write(worker_stop_fd_, 1);
    worker_.join();
    return false;
  }
  return true;
}

int DeviceMonitor::stopFd() const noexcept
{
  return stop_fd_;
}

void DeviceMonitor::monitorLoop()
{
  if (setpriority(PRIO_PROCESS, 0, 10) < 0)
    SPDLOG_DEBUG("Unable to lower DeviceMonitor thread priority: {}", std::strerror(errno));

  std::optional<Clock::time_point> deadline;
  bool notify_session = false;

  while (true)
  {
    const int timeout = deadline ? timeoutUntil(*deadline) : -1;
    pollfd descriptors[]{ { source_->fd(), POLLIN, 0 }, { worker_stop_fd_, POLLIN, 0 } };
    const int poll_result = poll(descriptors, 2, timeout);
    if (poll_result < 0)
    {
      if (errno == EINTR)
        continue;
      SPDLOG_ERROR("udev monitor poll failed: {}", std::strerror(errno));
      notify_session = true;
      break;
    }
    if (descriptors[1].revents & POLLIN)
      break;

    if (descriptors[0].revents & (POLLHUP | POLLNVAL))
    {
      SPDLOG_ERROR("udev input monitor became unavailable");
      notify_session = true;
      break;
    }
    if (descriptors[0].revents & POLLERR)
    {
      SPDLOG_WARN("udev input monitor reported an overflow or socket error");
      deadline = Clock::now();
    }
    if (descriptors[0].revents & POLLIN)
    {
      const DrainResult drain_result = drainUdevEvents(*source_, config_, relevant_keyboard_keys_);
      if (drain_result == DrainResult::Error)
      {
        notify_session = true;
        break;
      }
      if (drain_result == DrainResult::Changed)
        deadline = Clock::now() + config_.debounce;
    }

    if (deadline && Clock::now() >= *deadline)
    {
      notify_session = true;
      break;
    }
  }

  if (notify_session && eventfd_write(stop_fd_, 1) < 0)
    SPDLOG_ERROR("Failed to notify the active Session: {}", std::strerror(errno));
}

DeviceManager::DeviceManager(DeviceConfig config, const std::vector<unsigned int>& relevant_keyboard_keys,
                             const std::atomic_bool& shutdown, int shutdown_fd)
  : config_{ std::move(config) }
  , relevant_keyboard_keys_{ relevant_keyboard_keys }
  , shutdown_{ shutdown }
  , shutdown_fd_{ shutdown_fd }
{
  std::sort(relevant_keyboard_keys_.begin(), relevant_keyboard_keys_.end());
  relevant_keyboard_keys_.erase(std::unique(relevant_keyboard_keys_.begin(), relevant_keyboard_keys_.end()),
                                relevant_keyboard_keys_.end());
}

std::vector<std::string> DeviceManager::listEventDevicePaths() const
{
  std::vector<std::string> paths;
  DIR* directory = opendir("/dev/input");
  if (!directory)
  {
    SPDLOG_ERROR("Failed to open /dev/input: {}", std::strerror(errno));
    return paths;
  }

  while (dirent* entry = readdir(directory))
  {
    const std::string name = entry->d_name;
    if (name.compare(0, 5, "event") == 0)
      paths.push_back("/dev/input/" + name);
  }
  closedir(directory);
  std::sort(paths.begin(), paths.end());
  return paths;
}

std::vector<InputDeviceHandle> DeviceManager::acquireKeyboards(const std::string& mouse_path) const
{
  std::vector<InputDeviceHandle> keyboards;
  if (relevant_keyboard_keys_.empty())
    return keyboards;

  for (const std::string& path : listEventDevicePaths())
  {
    if (path == mouse_path)
      continue;
    auto handle = openInputDevice(path);
    if (!handle || handle->info().isVirtualDevice())
      continue;
    if (isIgnoredDevice(handle->info(), config_.ignored_devices) ||
        !handle->info().capabilities.supportsAnyKey(relevant_keyboard_keys_))
      continue;

    SPDLOG_INFO("Use keyboard device: {} ({})", handle->info().name, handle->info().path);
    keyboards.push_back(std::move(*handle));
  }
  return keyboards;
}

AcquireResult DeviceManager::acquireSessionDevices()
{
  // Subscribe before the first snapshot so an add racing with enumeration is either present in the snapshot or
  // remains queued here and forces another snapshot.
  auto source = UdevInputSource::create();
  if (!source)
    return { AcquireStatus::FatalError, std::nullopt };

  const auto wait_for_debounce = [&]() -> DebounceResult {
    auto deadline = Clock::now() + config_.debounce;
    while (!shutdown_.load(std::memory_order_relaxed))
    {
      pollfd descriptors[]{ { source->fd(), POLLIN, 0 }, { shutdown_fd_, POLLIN, 0 } };
      const int poll_result = poll(descriptors, 2, timeoutUntil(deadline));
      if (poll_result < 0)
      {
        if (errno == EINTR)
          continue;
        SPDLOG_ERROR("udev debounce wait failed: {}", std::strerror(errno));
        return DebounceResult::Error;
      }
      if (descriptors[1].revents & POLLIN)
        return DebounceResult::Shutdown;
      if (descriptors[0].revents & (POLLHUP | POLLNVAL))
      {
        SPDLOG_ERROR("udev input monitor failed while waiting for devices to settle");
        return DebounceResult::Error;
      }
      if (descriptors[0].revents & (POLLIN | POLLERR))
      {
        const DrainResult drain_result = drainUdevEvents(*source, config_, relevant_keyboard_keys_);
        if (drain_result == DrainResult::Error)
          return DebounceResult::Error;
        if (drain_result == DrainResult::Unchanged && (descriptors[0].revents & POLLERR))
        {
          SPDLOG_ERROR("udev input monitor reported an unrecoverable socket error");
          return DebounceResult::Error;
        }
        if (drain_result == DrainResult::Changed)
          deadline = Clock::now() + config_.debounce;
      }
      if (Clock::now() >= deadline)
        return DebounceResult::Settled;
    }
    return DebounceResult::Shutdown;
  };

  while (!shutdown_.load(std::memory_order_relaxed))
  {
    std::vector<InputDeviceHandle> candidates;
    for (const std::string& path : listEventDevicePaths())
    {
      auto handle = openInputDevice(path);
      if (!handle || handle->info().isVirtualDevice())
        continue;
      if (!isIgnoredDevice(handle->info(), config_.ignored_devices) && handle->info().capabilities.isMouse())
        candidates.push_back(std::move(*handle));
    }

    if (candidates.empty())
      SPDLOG_INFO("Waiting for a compatible mouse device...");
    else if (candidates.size() > 1)
      SPDLOG_INFO("Detecting the active mouse from {} candidate(s)...", candidates.size());

    std::optional<InputDeviceHandle> mouse;
    bool rebuild_snapshot = false;
    while (!shutdown_.load(std::memory_order_relaxed) && !mouse)
    {
      if (candidates.size() == 1)
      {
        SPDLOG_INFO("Use the only compatible mouse: {}", candidates.front().info().path);
        mouse.emplace(std::move(candidates.front()));
        break;
      }

      std::vector<pollfd> descriptors;
      descriptors.reserve(candidates.size() + 2);
      for (const InputDeviceHandle& candidate : candidates)
        descriptors.push_back({ candidate.fd(), POLLIN, 0 });
      const std::size_t udev_index = descriptors.size();
      descriptors.push_back({ source->fd(), POLLIN, 0 });
      const std::size_t shutdown_index = descriptors.size();
      descriptors.push_back({ shutdown_fd_, POLLIN, 0 });

      const int poll_result = poll(descriptors.data(), descriptors.size(), -1);
      if (poll_result < 0)
      {
        if (errno == EINTR)
          continue;
        SPDLOG_ERROR("Device acquisition poll failed: {}", std::strerror(errno));
        return { AcquireStatus::FatalError, std::nullopt };
      }
      if (descriptors[shutdown_index].revents & POLLIN)
        return { AcquireStatus::Shutdown, std::nullopt };
      if (descriptors[udev_index].revents & (POLLHUP | POLLNVAL))
      {
        SPDLOG_ERROR("udev input monitor became unavailable during device acquisition");
        return { AcquireStatus::FatalError, std::nullopt };
      }
      if (descriptors[udev_index].revents & (POLLIN | POLLERR))
      {
        const DrainResult drain_result = drainUdevEvents(*source, config_, relevant_keyboard_keys_);
        if (drain_result == DrainResult::Error)
          return { AcquireStatus::FatalError, std::nullopt };
        if (drain_result == DrainResult::Unchanged && (descriptors[udev_index].revents & POLLERR))
        {
          SPDLOG_ERROR("udev input monitor reported an unrecoverable socket error");
          return { AcquireStatus::FatalError, std::nullopt };
        }
        if (drain_result == DrainResult::Changed)
        {
          const DebounceResult debounce_result = wait_for_debounce();
          if (debounce_result == DebounceResult::Shutdown)
            return { AcquireStatus::Shutdown, std::nullopt };
          if (debounce_result == DebounceResult::Error)
            return { AcquireStatus::FatalError, std::nullopt };
          rebuild_snapshot = true;
          break;
        }
      }

      for (std::size_t index = 0; index < candidates.size(); ++index)
      {
        if (!(descriptors[index].revents & (POLLIN | POLLERR | POLLHUP | POLLNVAL)))
          continue;

        input_event event{};
        int result = 0;
        int read_flag = LIBEVDEV_READ_FLAG_NORMAL;
        while (true)
        {
          result = libevdev_next_event(candidates[index].evdev(), read_flag, &event);
          if (result == LIBEVDEV_READ_STATUS_SYNC)
            read_flag = LIBEVDEV_READ_FLAG_SYNC;
          else if (result != LIBEVDEV_READ_STATUS_SUCCESS)
            break;

          if (event.type == EV_REL)
          {
            SPDLOG_INFO("Active mouse detected: {}", candidates[index].info().path);
            mouse.emplace(std::move(candidates[index]));
            break;
          }
        }
        if (mouse)
          break;

        if (result == -ENODEV || (descriptors[index].revents & (POLLERR | POLLHUP | POLLNVAL)))
        {
          SPDLOG_DEBUG("Mouse candidate lost: {}", candidates[index].info().path);
          candidates.erase(candidates.begin() + static_cast<std::ptrdiff_t>(index));
          break;
        }
        if (result < 0 && result != -EAGAIN)
        {
          SPDLOG_WARN("Discarding unreadable mouse candidate {}: {}", candidates[index].info().path,
                      std::strerror(-result));
          candidates.erase(candidates.begin() + static_cast<std::ptrdiff_t>(index));
          break;
        }
      }
    }

    if (shutdown_.load(std::memory_order_relaxed))
      return { AcquireStatus::Shutdown, std::nullopt };
    if (rebuild_snapshot || !mouse)
      continue;

    const std::string mouse_path = mouse->info().path;
    SessionDevices devices;
    devices.mouse = std::move(*mouse);
    devices.keyboards = acquireKeyboards(mouse_path);

    const DrainResult final_drain = drainUdevEvents(*source, config_, relevant_keyboard_keys_);
    if (final_drain == DrainResult::Error)
      return { AcquireStatus::FatalError, std::nullopt };
    if (final_drain == DrainResult::Changed)
    {
      const DebounceResult debounce_result = wait_for_debounce();
      if (debounce_result == DebounceResult::Shutdown)
        return { AcquireStatus::Shutdown, std::nullopt };
      if (debounce_result == DebounceResult::Error)
        return { AcquireStatus::FatalError, std::nullopt };
      continue;
    }

    // Transfer the same subscribed socket to the runtime worker. No receiver runs concurrently, and events arriving
    // across this handoff remain queued on the socket.
    auto monitor = DeviceMonitor::start(std::move(source), config_, relevant_keyboard_keys_);
    if (!monitor)
      return { AcquireStatus::FatalError, std::nullopt };

    SessionResources resources{ std::move(devices), std::move(monitor) };
    return { AcquireStatus::Success, std::move(resources) };
  }

  return { AcquireStatus::Shutdown, std::nullopt };
}

}  // namespace smooth_scroll
