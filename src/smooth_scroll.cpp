// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Wayne6530

#include <atomic>
#include <optional>
#include <chrono>
#include <string_view>
#include <string>
#include <vector>

#include <dirent.h>
#include <libevdev-1.0/libevdev/libevdev.h>
#include <linux/uinput.h>
#include <signal.h>
#include <spdlog/spdlog.h>
#include <spdlog/fmt/ranges.h>
#include <toml++/toml.hpp>

#include "wheel_smoother.h"
#include "version.h"

using namespace std::string_view_literals;
using namespace smooth_scroll;

constexpr std::string_view kHelpStr =
    R"(Smooth Scroll for Linux (https://github.com/Wayne6530/smooth-scroll-linux)

Usage: smooth-scroll [options]

Options:
  -c, --config <file>  Specify config file path (default "./smooth-scroll.toml")
  -h, --help           Show help message
  -v, --version        Show version information
  -d, --debug          Enable debug mode (verbose logging for parameter tuning)
)"sv;

constexpr std::string_view kDefaultConfigPath = "./smooth-scroll.toml"sv;

std::atomic_bool kShutdown{ false };

void signalHandler(int signal_num)
{
  if (signal_num == SIGINT)
  {
    kShutdown.store(true, std::memory_order_relaxed);
  }
}

std::string findDevice()
{
  std::vector<std::pair<std::string, int>> mouse_devices;

  DIR* dir = opendir("/dev/input");
  if (!dir)
  {
    SPDLOG_ERROR("Failed to open /dev/input directory");
    return "";
  }

  dirent* entry;
  while ((entry = readdir(dir)) != nullptr)
  {
    std::string_view name = entry->d_name;
    if (name.rfind("event", 0) == 0)
    {
      std::string path = "/dev/input/" + std::string(name);

      int fd = open(path.c_str(), O_RDONLY | O_NONBLOCK);
      if (fd < 0)
      {
        SPDLOG_WARN("Failed to open device {}: {}", path, strerror(errno));
        continue;
      }

      libevdev* dev = nullptr;
      int rc = libevdev_new_from_fd(fd, &dev);
      if (rc < 0)
      {
        SPDLOG_WARN("Failed to initialize libevdev for {}: {}", path, strerror(-rc));
        close(fd);
        continue;
      }

      bool is_mouse = libevdev_has_event_type(dev, EV_REL) && libevdev_has_event_code(dev, EV_REL, REL_X) &&
                      libevdev_has_event_code(dev, EV_REL, REL_Y) && libevdev_has_event_code(dev, EV_REL, REL_WHEEL);

      libevdev_free(dev);

      if (is_mouse)
      {
        SPDLOG_DEBUG("Found mouse device: {}", path);
        mouse_devices.emplace_back(path, fd);
      }
      else
      {
        SPDLOG_DEBUG("Device {} is not a mouse", path);
        close(fd);
      }
    }
  }
  closedir(dir);

  if (mouse_devices.empty())
  {
    SPDLOG_ERROR("No mouse devices found");
    return "";
  }

  SPDLOG_INFO("Detecting active device...");

  std::chrono::microseconds deadline = std::chrono::duration_cast<std::chrono::microseconds>(
      (std::chrono::system_clock::now() + std::chrono::seconds{ 10 }).time_since_epoch());
  fd_set read_fds;
  while (!kShutdown.load(std::memory_order_relaxed))
  {
    FD_ZERO(&read_fds);
    int max_fd = -1;

    for (auto& [path, fd] : mouse_devices)
    {
      if (fd < 0)
        continue;

      FD_SET(fd, &read_fds);
      if (fd > max_fd)
        max_fd = fd;
    }

    if (max_fd < 0)
    {
      SPDLOG_INFO("All devices lost");
      break;
    }

    std::chrono::microseconds now =
        std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::system_clock::now().time_since_epoch());

    auto usec = (deadline - now).count();

    if (usec < 0)
    {
      SPDLOG_INFO("No active device detected");
      break;
    }

    struct timeval timeout;
    timeout.tv_sec = 0;
    timeout.tv_usec = usec;

    while (timeout.tv_usec >= 1'000'000)
    {
      timeout.tv_sec += 1;
      timeout.tv_usec -= 1'000'000;
    }

    int ret = select(max_fd + 1, &read_fds, nullptr, nullptr, &timeout);
    if (ret < 0)
    {
      if (errno == EINTR)
        continue;
      SPDLOG_ERROR("select error: {}", strerror(errno));
      break;
    }
    else if (ret == 0)
    {
      SPDLOG_INFO("No active device detected");
      break;
    }

    for (auto& [path, fd] : mouse_devices)
    {
      if (fd < 0)
        continue;

      if (FD_ISSET(fd, &read_fds))
      {
        libevdev* dev = nullptr;
        int rc = libevdev_new_from_fd(fd, &dev);
        if (rc < 0)
        {
          SPDLOG_INFO("Device {} lost", path);
          close(fd);
          fd = -1;
          continue;
        }

        int result;
        struct input_event ev;
        while ((result = libevdev_next_event(dev, LIBEVDEV_READ_FLAG_NORMAL, &ev)) == LIBEVDEV_READ_STATUS_SUCCESS)
        {
          if (ev.type == EV_REL)
          {
            SPDLOG_INFO("Active device detected: {}", path);
            libevdev_free(dev);

            for (auto& [path, fd] : mouse_devices)
            {
              if (fd < 0)
                continue;

              close(fd);
            }

            return path;
          }
        }
        libevdev_free(dev);

        if (result == -ENODEV)
        {
          SPDLOG_INFO("Device {} lost", path);
          close(fd);
          fd = -1;
        }
      }
    }
  }

  for (auto& [path, fd] : mouse_devices)
  {
    if (fd < 0)
      continue;

    close(fd);
  }

  return "";
}

struct KeyboardDevice
{
  int fd;
  libevdev* evdev;
  int num_passthrough;
};

std::vector<KeyboardDevice> findKeyboardDevices(const std::vector<unsigned int>& keys, const std::string& mouse_device)
{
  std::vector<KeyboardDevice> keyboard_devices;

  if (keys.empty())
  {
    return keyboard_devices;
  }

  DIR* dir = opendir("/dev/input");
  if (!dir)
  {
    SPDLOG_ERROR("Failed to open /dev/input directory");
    return keyboard_devices;
  }

  dirent* entry;
  while ((entry = readdir(dir)) != nullptr)
  {
    std::string_view name = entry->d_name;
    if (name.rfind("event", 0) == 0)
    {
      std::string path = "/dev/input/" + std::string(name);

      if (path == mouse_device)
      {
        continue;
      }

      int fd = open(path.c_str(), O_RDONLY | O_NONBLOCK);
      if (fd < 0)
      {
        SPDLOG_WARN("Failed to open device {}: {}", path, strerror(errno));
        continue;
      }

      libevdev* dev = nullptr;
      int rc = libevdev_new_from_fd(fd, &dev);
      if (rc < 0)
      {
        SPDLOG_WARN("Failed to initialize libevdev for {}: {}", path, strerror(-rc));
        close(fd);
        continue;
      }

      bool has_keys = false;

      bool is_keyboard = libevdev_has_event_type(dev, EV_KEY);
      if (is_keyboard)
      {
        for (auto key : keys)
        {
          if (libevdev_has_event_code(dev, EV_KEY, key))
          {
            has_keys = true;
            break;
          }
        }
      }

      if (has_keys)
      {
        SPDLOG_INFO("Use keyboard device: {}", path);
        keyboard_devices.push_back(KeyboardDevice{ fd, dev, 0 });
      }
      else
      {
        SPDLOG_DEBUG("Device {} is not a valid keyboard", path);
        close(fd);
        libevdev_free(dev);
      }
    }
  }
  closedir(dir);

  return keyboard_devices;
}

void waitUntilAllButtonsReleased(libevdev* evdev, const std::vector<int>& supported_buttons)
{
  auto are_any_buttons_pressed = [&]() -> bool {
    for (int btn : supported_buttons)
    {
      if (libevdev_get_event_value(evdev, EV_KEY, btn) != 0)
      {
        return true;
      }
    }
    return false;
  };

  while (!kShutdown.load(std::memory_order_relaxed) && are_any_buttons_pressed())
  {
    struct input_event ev;

    int rc = libevdev_next_event(evdev, LIBEVDEV_READ_FLAG_NORMAL | LIBEVDEV_READ_FLAG_BLOCKING, &ev);

    if (rc == LIBEVDEV_READ_STATUS_SUCCESS)
    {
      continue;
    }
    else if (rc == LIBEVDEV_READ_STATUS_SYNC)
    {
      while (rc == LIBEVDEV_READ_STATUS_SYNC)
      {
        rc = libevdev_next_event(evdev, LIBEVDEV_READ_FLAG_SYNC, &ev);
      }
    }
    else if (rc < 0 && rc != -EAGAIN)
    {
      break;
    }
  }
}

int main(int argc, char* argv[])
{
  spdlog::set_pattern("[%E.%f] [%^%L%$] %v");

  std::string config_path(kDefaultConfigPath);
  bool show_help = false;
  bool show_version = false;

  for (int i = 1; i < argc; ++i)
  {
    std::string_view arg = argv[i];
    if (arg == "-h" || arg == "--help")
    {
      show_help = true;
      break;
    }
    else if (arg == "-v" || arg == "--version")
    {
      show_version = true;
      break;
    }
    else if (arg == "-d" || arg == "--debug")
    {
      spdlog::set_level(spdlog::level::debug);
    }
    else if ((arg == "-c" || arg == "--config"))
    {
      if (i + 1 < argc)
      {
        config_path = argv[++i];
      }
      else
      {
        show_help = true;
        break;
      }
    }
    else
    {
      show_help = true;
      break;
    }
  }

  if (show_help)
  {
    fmt::print("{}", kHelpStr);
    return 0;
  }

  if (show_version)
  {
    fmt::print("{}\n", kVersion);
    return 0;
  }

  if (access(config_path.c_str(), R_OK) != 0)
  {
    SPDLOG_INFO("Config file '{}' is not readable: {}", config_path, strerror(errno));
  }

  toml::table table;
  try
  {
    table = toml::parse_file(config_path);
  }
  catch (const toml::parse_error& err)
  {
    SPDLOG_WARN("Parsing failed: {}", err.description());
  }

  std::optional<std::string> device = table["device"].value<std::string>();
  if (!device.has_value())
  {
    SPDLOG_INFO("No 'device' field in config file");

    device = findDevice();
    if ((*device).empty())
    {
      return -1;
    }
  }

  int free_spin_button = BTN_RIGHT;
  if (auto opt = table["free_spin_button"].value<int>())
  {
    free_spin_button = *opt;
    SPDLOG_INFO("Use free spin button {}", free_spin_button);
  }
  else
  {
    SPDLOG_WARN("Use default free spin button {}", free_spin_button);
  }

  int drag_view_button = BTN_LEFT;
  if (auto opt = table["drag_view_button"].value<int>())
  {
    drag_view_button = *opt;
    SPDLOG_INFO("Use drag view button {}", drag_view_button);
  }
  else
  {
    SPDLOG_WARN("Use default drag view button {}", drag_view_button);
  }

  std::vector<unsigned int> keyboard_braking_keys;
  if (auto array = table["keyboard_braking_keys"].as_array())
  {
    for (auto& elem : *array)
    {
      if (auto val = elem.value<unsigned int>())
      {
        if (*val < KEY_CNT)
          keyboard_braking_keys.push_back(*val);
      }
    }
  }
  SPDLOG_INFO("Use keyboard braking keys {}", keyboard_braking_keys);

  std::vector<unsigned int> keyboard_passthrough_keys;
  if (auto array = table["keyboard_passthrough_keys"].as_array())
  {
    for (auto& elem : *array)
    {
      if (auto val = elem.value<unsigned int>())
      {
        if (*val < KEY_CNT)
          keyboard_passthrough_keys.push_back(*val);
      }
    }
  }
  SPDLOG_INFO("Use keyboard passthrough keys {}", keyboard_passthrough_keys);

  auto read_option = [&table](const char* name, auto& value) {
    if (auto opt = table[name].value<std::remove_reference_t<decltype(value)>>())
    {
      value = *opt;
      SPDLOG_INFO("Config loaded: {} = {}", name, value);
    }
    else
    {
      SPDLOG_WARN("Config '{}' not found or invalid, using default: {}", name, value);
    }
  };

  WheelSmoother::Options options;
  read_option("tick_interval_microseconds", options.tick_interval_microseconds);
  read_option("min_deceleration", options.min_deceleration);
  read_option("max_deceleration", options.max_deceleration);
  read_option("initial_speed", options.initial_speed);
  read_option("speed_factor", options.speed_factor);
  read_option("speed_smooth_window_microseconds", options.speed_smooth_window_microseconds);
  read_option("max_speed_change_lowerbound", options.max_speed_change_lowerbound);
  read_option("min_speed_change_upperbound", options.min_speed_change_upperbound);
  read_option("min_speed_change_ratio", options.min_speed_change_ratio);
  read_option("max_speed_change_ratio", options.max_speed_change_ratio);
  read_option("damping", options.damping);
  read_option("use_reverse_scroll_braking", options.use_reverse_scroll_braking);
  read_option("max_reverse_scroll_braking_microseconds", options.max_reverse_scroll_braking_microseconds);
  read_option("max_reverse_scroll_braking_times", options.max_reverse_scroll_braking_times);
  read_option("use_mouse_movement_braking", options.use_mouse_movement_braking);
  read_option("max_mouse_movement_distance", options.max_mouse_movement_distance);
  read_option("mouse_movement_window_milliseconds", options.mouse_movement_window_milliseconds);
  read_option("mouse_movement_delay_microseconds", options.mouse_movement_delay_microseconds);
  read_option("drag_view_speed", options.drag_view_speed);

  if (signal(SIGINT, signalHandler) == SIG_ERR)
  {
    SPDLOG_ERROR("can't catch SIGINT");
    return -1;
  }

  int mouse_fd = open((*device).c_str(), O_RDONLY | O_NONBLOCK);
  if (mouse_fd < 0)
  {
    SPDLOG_ERROR("can't open {}", *device);
    return -1;
  }

  struct libevdev* mouse_evdev = nullptr;
  int rc = libevdev_new_from_fd(mouse_fd, &mouse_evdev);
  if (rc < 0)
  {
    SPDLOG_ERROR("failed to initialize libevdev: {}", strerror(-rc));
    close(mouse_fd);
    return -1;
  }

  int uinput_fd = open("/dev/uinput", O_WRONLY | O_NONBLOCK);
  if (uinput_fd < 0)
  {
    SPDLOG_ERROR("failed to open /dev/uinput");
    libevdev_free(mouse_evdev);
    close(mouse_fd);
    return -1;
  }

  std::vector<int> supported_buttons;

  SPDLOG_INFO("Input device name: \"{}\"", libevdev_get_name(mouse_evdev));
  SPDLOG_INFO("Input device ID: bus {:#x} vendor {:#x} product {:#x}", libevdev_get_id_bustype(mouse_evdev),
              libevdev_get_id_vendor(mouse_evdev), libevdev_get_id_product(mouse_evdev));

  for (int type = 0; type < EV_MAX; type++)
  {
    if (libevdev_has_event_type(mouse_evdev, type))
    {
      const char* type_name = libevdev_event_type_get_name(type);
      SPDLOG_INFO("  Event type {} ({}) supported", type, type_name ? type_name : "?");

      if (type == EV_KEY)
      {
        ioctl(uinput_fd, UI_SET_EVBIT, type);
        for (int code = 0; code < KEY_MAX; code++)
        {
          if (libevdev_has_event_code(mouse_evdev, type, code))
          {
            const char* code_name = libevdev_event_code_get_name(type, code);
            SPDLOG_INFO("    Event code {} ({})", code, code_name ? code_name : "?");
            ioctl(uinput_fd, UI_SET_KEYBIT, code);
            supported_buttons.push_back(code);
          }
        }
      }
      else if (type == EV_REL)
      {
        ioctl(uinput_fd, UI_SET_EVBIT, type);
        for (int code = 0; code < REL_MAX; code++)
        {
          if (libevdev_has_event_code(mouse_evdev, type, code))
          {
            const char* code_name = libevdev_event_code_get_name(type, code);
            SPDLOG_INFO("    Event code {} ({})", code, code_name ? code_name : "?");
            ioctl(uinput_fd, UI_SET_RELBIT, code);
          }
        }
      }
      else if (type == EV_MSC)
      {
        ioctl(uinput_fd, UI_SET_EVBIT, type);
        for (int code = 0; code < MSC_MAX; code++)
        {
          if (libevdev_has_event_code(mouse_evdev, type, code))
          {
            const char* code_name = libevdev_event_code_get_name(type, code);
            SPDLOG_INFO("    Event code {} ({})", code, code_name ? code_name : "?");
            ioctl(uinput_fd, UI_SET_MSCBIT, code);
          }
        }
      }
    }
  }

  struct uinput_user_dev uidev;
  memset(&uidev, 0, sizeof(uidev));
  snprintf(uidev.name, UINPUT_MAX_NAME_SIZE, "Virtual Smooth Mouse");
  uidev.id.bustype = BUS_USB;
  uidev.id.vendor = 0x1234;
  uidev.id.product = 0x5678;
  uidev.id.version = 1;

  if (write(uinput_fd, &uidev, sizeof(uidev)) < 0)
  {
    SPDLOG_ERROR("Write uidev failed");
    close(uinput_fd);
    libevdev_free(mouse_evdev);
    close(mouse_fd);
    return -1;
  }

  if (ioctl(uinput_fd, UI_DEV_CREATE) < 0)
  {
    SPDLOG_ERROR("Unable to create uinput device");
    close(uinput_fd);
    libevdev_free(mouse_evdev);
    close(mouse_fd);
    return -1;
  }

  int num_passthrough = 0;
  std::array<bool, KEY_CNT> braking_keys_table{};
  std::array<bool, KEY_CNT> passthrough_keys_table{};
  std::vector<KeyboardDevice> keyboard_devices;
  if (!keyboard_braking_keys.empty() || !keyboard_passthrough_keys.empty())
  {
    std::vector<unsigned int> keys;
    keys.reserve(keyboard_braking_keys.size() + keyboard_passthrough_keys.size());
    keys.insert(keys.end(), keyboard_braking_keys.begin(), keyboard_braking_keys.end());
    keys.insert(keys.end(), keyboard_passthrough_keys.begin(), keyboard_passthrough_keys.end());

    for (auto key : keys)
    {
      braking_keys_table[key] = true;
    }

    for (auto key : keyboard_passthrough_keys)
    {
      passthrough_keys_table[key] = true;
    }

    keyboard_devices = findKeyboardDevices(keys, *device);
  }

  waitUntilAllButtonsReleased(mouse_evdev, supported_buttons);

  if (libevdev_grab(mouse_evdev, LIBEVDEV_GRAB) < 0)
  {
    SPDLOG_ERROR("failed to grab mouse_evdev");

    ioctl(uinput_fd, UI_DEV_DESTROY);
    close(uinput_fd);
    libevdev_free(mouse_evdev);
    close(mouse_fd);
    for (const auto& dev : keyboard_devices)
    {
      libevdev_free(dev.evdev);
      close(dev.fd);
    }
    return -1;
  }

  WheelSmoother wheel_smoother{ options };

  int max_fd = mouse_fd;
  fd_set fds;
  FD_ZERO(&fds);
  FD_SET(mouse_fd, &fds);
  for (const auto& dev : keyboard_devices)
  {
    FD_SET(dev.fd, &fds);
    if (dev.fd > max_fd)
    {
      max_fd = dev.fd;
    }
  }

  bool drop_syn_report = false;
  struct input_event ev;
  while (!kShutdown.load(std::memory_order_relaxed))
  {
    fd_set read_fds = fds;

    auto timeout = wheel_smoother.timeout();

    int select_ret = select(max_fd + 1, &read_fds, NULL, NULL, timeout.has_value() ? &timeout.value() : NULL);
    if (select_ret < 0)
    {
      if (errno == EINTR)
      {
        SPDLOG_TRACE("select errno EINTR");
        continue;
      }

      SPDLOG_ERROR("select error: {}", select_ret);
      break;
    }
    else if (select_ret == 0)
    {
      auto ev_wheel = wheel_smoother.tick();
      if (ev_wheel.has_value())
      {
        SPDLOG_TRACE("select timeout");

        ev = *ev_wheel;
        SPDLOG_TRACE("{}.{:0>6} type {} code {} value {}", ev.time.tv_sec, ev.time.tv_usec,
                     libevdev_event_type_get_name(ev.type), libevdev_event_code_get_name(ev.type, ev.code), ev.value);
        if (write(uinput_fd, &ev, sizeof(ev)) == -1)
        {
          SPDLOG_ERROR("Write uinput failed");
          break;
        }

        ev.type = EV_SYN;
        ev.code = SYN_REPORT;
        ev.value = 0;

        SPDLOG_TRACE("{}.{:0>6} type {} code {} value {}", ev.time.tv_sec, ev.time.tv_usec,
                     libevdev_event_type_get_name(ev.type), libevdev_event_code_get_name(ev.type, ev.code), ev.value);
        if (write(uinput_fd, &ev, sizeof(ev)) == -1)
        {
          SPDLOG_ERROR("Write uinput failed");
          break;
        }
      }
      continue;
    }

    for (auto it = keyboard_devices.begin(); it != keyboard_devices.end();)
    {
      const unsigned int fd = it->fd;

      if (!FD_ISSET(fd, &read_fds))
      {
        ++it;
        continue;
      }

      libevdev* evdev = it->evdev;

      int result;
      int read_flag = LIBEVDEV_READ_FLAG_NORMAL;
      while (true)
      {
        result = libevdev_next_event(evdev, read_flag, &ev);

        if (result == LIBEVDEV_READ_STATUS_SYNC)
        {
          if (ev.type == EV_SYN && ev.code == SYN_DROPPED)
          {
            read_flag = LIBEVDEV_READ_FLAG_SYNC;
            continue;
          }
        }
        else if (result != LIBEVDEV_READ_STATUS_SUCCESS)
        {
          break;
        }

        if (ev.type == EV_KEY && ev.code < KEY_CNT && ev.value != 2)
        {
          if (braking_keys_table[ev.code])
          {
            wheel_smoother.stop();
          }

          if (passthrough_keys_table[ev.code])
          {
            if (ev.value == 1)
            {
              ++it->num_passthrough;
              ++num_passthrough;
            }
            else
            {
              if (it->num_passthrough)
              {
                --it->num_passthrough;
                --num_passthrough;
              }
            }
          }
        }
      }

      if (result == -ENODEV)
      {
        SPDLOG_WARN("Keyboard device lost");
        libevdev_free(evdev);
        close(fd);
        num_passthrough -= it->num_passthrough;

        it = keyboard_devices.erase(it);

        max_fd = mouse_fd;
        FD_ZERO(&fds);
        FD_SET(mouse_fd, &fds);
        for (const auto& dev : keyboard_devices)
        {
          FD_SET(dev.fd, &fds);
          if (dev.fd > max_fd)
          {
            max_fd = dev.fd;
          }
        }

        continue;
      }

      ++it;
    }

    if (FD_ISSET(mouse_fd, &read_fds))
    {
      int result;
      int read_flag = LIBEVDEV_READ_FLAG_NORMAL;
      while (true)
      {
        result = libevdev_next_event(mouse_evdev, read_flag, &ev);

        if (result == LIBEVDEV_READ_STATUS_SYNC)
        {
          if (ev.type == EV_SYN && ev.code == SYN_DROPPED)
          {
            read_flag = LIBEVDEV_READ_FLAG_SYNC;
            continue;
          }
        }
        else if (result != LIBEVDEV_READ_STATUS_SUCCESS)
        {
          break;
        }

        bool should_write = true;

        switch (ev.type)
        {
          case EV_REL:
            switch (ev.code)
            {
              case REL_WHEEL:
              case REL_HWHEEL:
                if (!num_passthrough)
                {
                  auto ev_wheel = wheel_smoother.handleEvent(ev.time, ev.value > 0, ev.code == REL_HWHEEL);
                  if (ev_wheel.has_value())
                  {
                    ev = *ev_wheel;
                  }
                  else
                  {
                    drop_syn_report = true;
                    should_write = false;
                  }
                }
                break;

              case REL_WHEEL_HI_RES:
              case REL_HWHEEL_HI_RES:
                if (!num_passthrough)
                {
                  should_write = false;
                }
                break;

              case REL_X:
                wheel_smoother.handleRelXEvent(ev);
                break;

              case REL_Y:
                wheel_smoother.handleRelYEvent(ev);
                break;

              default:
                break;
            }
            break;

          case EV_KEY: {
            bool handled = false;

            if (ev.code == drag_view_button)
            {
              handled = wheel_smoother.handleDragViewButton(ev.value);
            }
            else if (ev.code == free_spin_button)
            {
              handled = wheel_smoother.handleFreeSpinButton(ev.value);
            }

            if (handled)
            {
              drop_syn_report = true;
              should_write = false;
            }
            else
            {
              wheel_smoother.stop();
            }
            break;
          }

          case EV_MSC:
            should_write = false;
            break;

          case EV_SYN:
            if (ev.code == SYN_REPORT)
            {
              if (drop_syn_report)
              {
                drop_syn_report = false;
                should_write = false;
              }
              else
              {
                wheel_smoother.handleReportEvent(ev.time);
              }
            }
            break;

          default:
            break;
        }

        if (should_write)
        {
          SPDLOG_TRACE("{}.{:0>6} type {} code {} value {}", ev.time.tv_sec, ev.time.tv_usec,
                       libevdev_event_type_get_name(ev.type), libevdev_event_code_get_name(ev.type, ev.code), ev.value);

          if (write(uinput_fd, &ev, sizeof(ev)) == -1)
          {
            SPDLOG_ERROR("Write uinput failed");
            break;
          }
        }
      }

      if (result == -ENODEV)
      {
        SPDLOG_ERROR("Mouse device lost");
        ioctl(uinput_fd, UI_DEV_DESTROY);
        close(uinput_fd);
        libevdev_free(mouse_evdev);
        close(mouse_fd);
        for (const auto& dev : keyboard_devices)
        {
          libevdev_free(dev.evdev);
          close(dev.fd);
        }
        return -1;
      }
    }

    if (ev.type == EV_SYN && ev.code == SYN_REPORT)
    {
      auto next_tick_time = wheel_smoother.next_tick_time();
      if (next_tick_time.has_value())
      {
        std::chrono::microseconds event_time =
            std::chrono::seconds{ ev.time.tv_sec } + std::chrono::microseconds{ ev.time.tv_usec };
        if (event_time > *next_tick_time)
        {
          auto ev_wheel = wheel_smoother.tick();
          if (ev_wheel.has_value())
          {
            SPDLOG_TRACE("event_time > *next_tick_time");

            ev = *ev_wheel;
            SPDLOG_TRACE("{}.{:0>6} type {} code {} value {}", ev.time.tv_sec, ev.time.tv_usec,
                         libevdev_event_type_get_name(ev.type), libevdev_event_code_get_name(ev.type, ev.code),
                         ev.value);
            if (write(uinput_fd, &ev, sizeof(ev)) == -1)
            {
              SPDLOG_ERROR("Write uinput failed");
              break;
            }

            ev.type = EV_SYN;
            ev.code = SYN_REPORT;
            ev.value = 0;

            SPDLOG_TRACE("{}.{:0>6} type {} code {} value {}", ev.time.tv_sec, ev.time.tv_usec,
                         libevdev_event_type_get_name(ev.type), libevdev_event_code_get_name(ev.type, ev.code),
                         ev.value);
            if (write(uinput_fd, &ev, sizeof(ev)) == -1)
            {
              SPDLOG_ERROR("Write uinput failed");
              break;
            }
          }
        }
      }
    }
  }

  if (libevdev_grab(mouse_evdev, LIBEVDEV_UNGRAB) < 0)
  {
    SPDLOG_ERROR("failed to ungrab mouse_evdev");
    ioctl(uinput_fd, UI_DEV_DESTROY);
    close(uinput_fd);
    libevdev_free(mouse_evdev);
    close(mouse_fd);
    for (const auto& dev : keyboard_devices)
    {
      libevdev_free(dev.evdev);
      close(dev.fd);
    }
    return -1;
  }

  ioctl(uinput_fd, UI_DEV_DESTROY);
  close(uinput_fd);
  libevdev_free(mouse_evdev);
  close(mouse_fd);
  for (const auto& dev : keyboard_devices)
  {
    libevdev_free(dev.evdev);
    close(dev.fd);
  }
  return 0;
}
