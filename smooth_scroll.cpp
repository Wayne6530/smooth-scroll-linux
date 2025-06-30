#include <atomic>
#include <optional>
#include <chrono>

#include <libudev.h>
#include <libevdev-1.0/libevdev/libevdev.h>
#include <linux/uinput.h>
#include <signal.h>
#define SPDLOG_ACTIVE_LEVEL SPDLOG_LEVEL_TRACE
#include <spdlog/spdlog.h>

#include "wheel_smoother.h"

using namespace smooth_scroll;

std::atomic_bool shutdown{ false };

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
