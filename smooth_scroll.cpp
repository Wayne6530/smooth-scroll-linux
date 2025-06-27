#include <atomic>
#include <optional>
#include <chrono>

#include <libudev.h>
#include <libevdev-1.0/libevdev/libevdev.h>
#include <linux/uinput.h>
#include <signal.h>
#define SPDLOG_ACTIVE_LEVEL SPDLOG_LEVEL_TRACE
#include <spdlog/spdlog.h>

std::atomic_bool shutdown{ false };

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

  WheelSmoother(const Options& options)
    : options_{ options }
    , tick_interval_{ static_cast<double>(options.tick_interval_microseconds) / 1.e6 }
    , min_delta_{ options.min_speed * tick_interval_ < 1 ? 1 : options.min_speed * tick_interval_ }
    , initial_delta_{ options.initial_speed * tick_interval_ }
    , alpha_{ std::exp(-options.damping * tick_interval_) }
    , max_delta_increase_{ options.max_speed_increase_per_wheel_event * tick_interval_ }
    , max_delta_decrease_{ options.max_speed_decrease_per_wheel_event * tick_interval_ }
    , delta_decrease_per_braking_{ options.speed_decrease_per_braking * tick_interval_ }
    , braking_cut_off_delta_{ options_.braking_cut_off_speed * tick_interval_ }
  {
    SPDLOG_DEBUG("tick interval {}s alpha {}", tick_interval_, alpha_);
  }

  void stop()
  {
    if (delta_ != 0)
    {
      SPDLOG_DEBUG("click stop");
      delta_ = 0;
    }
  }

  std::optional<struct input_event> handleEvent(const struct timeval& time, bool positive)
  {
    std::chrono::microseconds event_time =
        std::chrono::seconds{ time.tv_sec } + std::chrono::microseconds{ time.tv_usec };

    if (delta_ == 0)
    {
      if (options_.use_braking)
      {
        if (positive == positive_ &&
            event_time < last_brake_stop_time_ + std::chrono::microseconds{ options_.braking_dejitter_microseconds })
        {
          SPDLOG_DEBUG("braking dejitter");
          return {};
        }
      }

      last_event_time_ = event_time;
      next_tick_time_ = event_time + std::chrono::microseconds{ options_.tick_interval_microseconds };
      last_brake_stop_time_ = std::chrono::microseconds{ 0 };

      positive_ = positive;
      delta_ = initial_delta_;

      SPDLOG_DEBUG("initial speed {:.2f}", delta_ / tick_interval_);

      int round_delta = std::round(delta_);
      deviation_ = delta_ - round_delta;

      struct input_event ev;
      ev.time = time;
      ev.type = EV_REL;
      ev.code = REL_WHEEL_HI_RES;
      ev.value = positive_ ? round_delta : -round_delta;

      return ev;
    }

    if ((positive && !positive_) || (!positive && positive_))
    {
      if (options_.use_braking)
      {
        delta_ -= delta_decrease_per_braking_;

        if (delta_ < braking_cut_off_delta_)
        {
          SPDLOG_DEBUG("braking stop");

          last_brake_stop_time_ = event_time;

          positive_ = positive;
          delta_ = 0;
        }
        else
        {
          SPDLOG_DEBUG("braking");
        }

        return {};
      }

      last_event_time_ = event_time;
      next_tick_time_ = event_time + std::chrono::microseconds{ options_.tick_interval_microseconds };
      last_brake_stop_time_ = std::chrono::microseconds{ 0 };

      positive_ = positive;
      delta_ = initial_delta_;

      SPDLOG_DEBUG("initial speed {:.2f}", delta_ / tick_interval_);

      int round_delta = std::round(delta_);
      deviation_ = delta_ - round_delta;

      struct input_event ev;
      ev.time = time;
      ev.type = EV_REL;
      ev.code = REL_WHEEL_HI_RES;
      ev.value = positive_ ? round_delta : -round_delta;

      return ev;
    }

    double speed = options_.speed_factor / std::chrono::duration<double>(event_time - last_event_time_).count();
    double delta = std::clamp(speed * tick_interval_, delta_ - max_delta_decrease_, delta_ + max_delta_increase_);

    last_event_time_ = event_time;
    next_tick_time_ = event_time + std::chrono::microseconds{ options_.tick_interval_microseconds };

    positive_ = positive;
    delta_ = delta < initial_delta_ ? initial_delta_ : delta;

    SPDLOG_DEBUG("set speed: actual {:.2f} target {:.2f}", delta_ / tick_interval_, speed);

    int round_delta = std::round(delta_);
    deviation_ = delta_ - round_delta;

    struct input_event ev;
    ev.time = time;
    ev.type = EV_REL;
    ev.code = REL_WHEEL_HI_RES;
    ev.value = positive_ ? round_delta : -round_delta;

    return ev;
  }

  std::optional<struct input_event> tick()
  {
    if (delta_ == 0)
    {
      return {};
    }

    delta_ *= alpha_;

    if (delta_ < min_delta_)
    {
      SPDLOG_DEBUG("damping stop");

      delta_ = 0;
      return {};
    }

    SPDLOG_TRACE("tick speed {:.2f}", delta_ / tick_interval_);

    std::chrono::microseconds current_tick_time = next_tick_time_;
    next_tick_time_ += std::chrono::microseconds{ options_.tick_interval_microseconds };

    int round_delta = std::round(delta_ + deviation_);
    deviation_ = delta_ + deviation_ - round_delta;

    if (round_delta == 0)
    {
      return {};
    }

    struct input_event ev;
    ev.time.tv_sec = std::chrono::duration_cast<std::chrono::seconds>(current_tick_time).count();
    ev.time.tv_usec = (current_tick_time - std::chrono::seconds{ ev.time.tv_sec }).count();
    ev.type = EV_REL;
    ev.code = REL_WHEEL_HI_RES;
    ev.value = positive_ ? round_delta : -round_delta;

    return ev;
  }

  std::optional<struct timeval> timeout()
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

    while (timeout.tv_usec > 1e6)
    {
      timeout.tv_sec += 1;
      timeout.tv_usec -= 1e6;
    }

    return timeout;
  }

  std::optional<std::chrono::microseconds> next_tick_time()
  {
    if (delta_ == 0)
    {
      return {};
    }

    return next_tick_time_;
  }

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

void signalHandler(int signal_num)
{
  if (signal_num == SIGINT)
  {
    shutdown.store(true, std::memory_order_relaxed);
  }
}

int main(int argc, char* argv[])
{
  spdlog::set_level(spdlog::level::debug);
  spdlog::set_pattern("[%E.%f] [%^%L%$] %v");

  if (signal(SIGINT, signalHandler) == SIG_ERR)
  {
    SPDLOG_ERROR("can't catch SIGINT");
    return -1;
  }

  if (argc < 2)
  {
    SPDLOG_INFO("usage: {} /dev/input/event*", argv[0]);
    return -1;
  }

  int fd = open(argv[1], O_RDONLY | O_NONBLOCK);
  if (fd < 0)
  {
    SPDLOG_ERROR("can't open {}", argv[1]);
    return -1;
  }

  struct libevdev* evdev = nullptr;
  int rc = libevdev_new_from_fd(fd, &evdev);
  if (rc < 0)
  {
    SPDLOG_ERROR("failed to initialize libevdev: {}", strerror(-rc));
    close(fd);
    return -1;
  }

  if (libevdev_grab(evdev, LIBEVDEV_GRAB) < 0)
  {
    SPDLOG_ERROR("failed to grab evdev");
    libevdev_free(evdev);
    close(fd);
    return -1;
  }

  int uinput_fd = open("/dev/uinput", O_WRONLY | O_NONBLOCK);
  if (uinput_fd < 0)
  {
    SPDLOG_ERROR("failed to open /dev/uinput");
    libevdev_free(evdev);
    close(fd);
    return -1;
  }

  SPDLOG_INFO("Input device name: \"{}\"", libevdev_get_name(evdev));
  SPDLOG_INFO("Input device ID: bus {:#x} vendor {:#x} product {:#x}", libevdev_get_id_bustype(evdev),
              libevdev_get_id_vendor(evdev), libevdev_get_id_product(evdev));

  for (int type = 0; type < EV_MAX; type++)
  {
    if (libevdev_has_event_type(evdev, type))
    {
      SPDLOG_INFO("  Event type {} ({}) supported", type, libevdev_event_type_get_name(type));

      if (type == EV_KEY)
      {
        ioctl(uinput_fd, UI_SET_EVBIT, type);
        for (int code = 0; code < KEY_MAX; code++)
        {
          if (libevdev_has_event_code(evdev, type, code))
          {
            SPDLOG_INFO("    Event code {} ({})", code, libevdev_event_code_get_name(type, code));
            ioctl(uinput_fd, UI_SET_KEYBIT, code);
          }
        }
      }
      else if (type == EV_REL)
      {
        ioctl(uinput_fd, UI_SET_EVBIT, type);
        for (int code = 0; code < REL_MAX; code++)
        {
          if (libevdev_has_event_code(evdev, type, code))
          {
            SPDLOG_INFO("    Event code {} ({})", code, libevdev_event_code_get_name(type, code));
            ioctl(uinput_fd, UI_SET_RELBIT, code);
          }
        }
      }
      else if (type == EV_MSC)
      {
        ioctl(uinput_fd, UI_SET_EVBIT, type);
        for (int code = 0; code < MSC_MAX; code++)
        {
          if (libevdev_has_event_code(evdev, type, code))
          {
            SPDLOG_INFO("    Event code {} ({})", code, libevdev_event_code_get_name(type, code));
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
    libevdev_free(evdev);
    close(fd);
    return -1;
  }

  if (ioctl(uinput_fd, UI_DEV_CREATE) < 0)
  {
    SPDLOG_ERROR("Unable to create uinput device");
    close(uinput_fd);
    libevdev_free(evdev);
    close(fd);
    return -1;
  }

  WheelSmoother wheel_smoother{ WheelSmoother::Options{} };

  bool drop_syn_report = false;
  struct input_event ev;
  while (!shutdown.load(std::memory_order_relaxed))
  {
    fd_set read_fds;
    FD_ZERO(&read_fds);
    FD_SET(fd, &read_fds);

    auto timeout = wheel_smoother.timeout();

    int select_ret = select(fd + 1, &read_fds, NULL, NULL, timeout.has_value() ? &timeout.value() : NULL);
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
        write(uinput_fd, &ev, sizeof(ev));

        ev.type = EV_SYN;
        ev.code = SYN_REPORT;
        ev.value = 0;

        SPDLOG_TRACE("{}.{:0>6} type {} code {} value {}", ev.time.tv_sec, ev.time.tv_usec,
                     libevdev_event_type_get_name(ev.type), libevdev_event_code_get_name(ev.type, ev.code), ev.value);
        write(uinput_fd, &ev, sizeof(ev));
      }
    }

    while (libevdev_next_event(evdev, LIBEVDEV_READ_FLAG_NORMAL, &ev) == LIBEVDEV_READ_STATUS_SUCCESS)
    {
      if (ev.type == EV_REL && ev.code == REL_WHEEL)
      {
        auto ev_wheel = wheel_smoother.handleEvent(ev.time, ev.value > 0);
        if (ev_wheel.has_value())
        {
          ev = *ev_wheel;

          SPDLOG_TRACE("{}.{:0>6} type {} code {} value {}", ev.time.tv_sec, ev.time.tv_usec,
                       libevdev_event_type_get_name(ev.type), libevdev_event_code_get_name(ev.type, ev.code), ev.value);
          write(uinput_fd, &ev, sizeof(ev));
        }
        else
        {
          drop_syn_report = true;
        }

        continue;
      }

      if (ev.type == EV_REL && ev.code == REL_WHEEL_HI_RES)
      {
        continue;
      }

      if (drop_syn_report)
      {
        if (ev.type == EV_SYN && ev.code == SYN_REPORT)
        {
          SPDLOG_TRACE("{}.{:0>6} type {} code {} value {} dropped", ev.time.tv_sec, ev.time.tv_usec,
                       libevdev_event_type_get_name(ev.type), libevdev_event_code_get_name(ev.type, ev.code), ev.value);
          drop_syn_report = false;
          continue;
        }
      }

      if (ev.type == EV_KEY)
      {
        wheel_smoother.stop();
      }

      SPDLOG_TRACE("{}.{:0>6} type {} code {} value {}", ev.time.tv_sec, ev.time.tv_usec,
                   libevdev_event_type_get_name(ev.type), libevdev_event_code_get_name(ev.type, ev.code), ev.value);
      write(uinput_fd, &ev, sizeof(ev));
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
            write(uinput_fd, &ev, sizeof(ev));

            ev.type = EV_SYN;
            ev.code = SYN_REPORT;
            ev.value = 0;

            SPDLOG_TRACE("{}.{:0>6} type {} code {} value {}", ev.time.tv_sec, ev.time.tv_usec,
                         libevdev_event_type_get_name(ev.type), libevdev_event_code_get_name(ev.type, ev.code),
                         ev.value);
            write(uinput_fd, &ev, sizeof(ev));
          }
        }
      }
    }
  }

  if (libevdev_grab(evdev, LIBEVDEV_UNGRAB) < 0)
  {
    SPDLOG_ERROR("failed to ungrab evdev");
    ioctl(uinput_fd, UI_DEV_DESTROY);
    close(uinput_fd);
    libevdev_free(evdev);
    close(fd);
    return -1;
  }

  ioctl(uinput_fd, UI_DEV_DESTROY);
  close(uinput_fd);
  libevdev_free(evdev);
  close(fd);
  return 0;
}
