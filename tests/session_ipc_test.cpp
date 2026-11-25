// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Wayne6530

#include "session.h"

#ifdef NDEBUG
#undef NDEBUG
#endif
#include <cassert>
#include <cerrno>
#include <iostream>
#include <utility>
#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>

namespace
{
// The keyboard pipe supplies complete evdev frames. The mouse has no queued events.
libevdev* keyboard_device;
int keyboard_fd;
int last_key_value = -1;
std::vector<std::pair<int, libevdev_grab_mode>> grab_changes;
}  // namespace

extern "C" int libevdev_next_event(libevdev* device, unsigned int, input_event* event)
{
  if (device != keyboard_device)
    return -EAGAIN;
  if (read(keyboard_fd, event, sizeof(*event)) != sizeof(*event))
    return -EAGAIN;
  if (event->type == EV_KEY)
    last_key_value = event->value;
  return LIBEVDEV_READ_STATUS_SUCCESS;
}

extern "C" int libevdev_grab(libevdev*, libevdev_grab_mode mode)
{
  grab_changes.emplace_back(last_key_value, mode);
  return 0;
}

namespace smooth_scroll
{

// Exercise the real request handler without acquiring physical input devices.
struct SessionIpcTestAccess
{
  static void testDirectModeRequests(IpcServer& server, SmoothScrollIPC& ipc)
  {
    SessionConfig config;
    config.profiles.resize(2);
    ProfileSet profiles{ config };
    VirtualDevice output;
    int keyboard_pipe[2];
    int mouse_pipe[2];
    assert(pipe2(keyboard_pipe, O_NONBLOCK) == 0);
    assert(pipe2(mouse_pipe, O_NONBLOCK) == 0);
    keyboard_device = libevdev_new();
    keyboard_fd = keyboard_pipe[0];
    DeviceInfo keyboard_info;
    keyboard_info.capabilities.keys[KEY_SCROLLLOCK] = true;
    SessionResources resources;
    resources.devices.mouse = InputDeviceHandle{ mouse_pipe[0], libevdev_new(), {} };
    resources.devices.keyboards.emplace_back(keyboard_fd, keyboard_device, std::move(keyboard_info));

    const std::atomic_bool shutdown{ false };
    Session::CreateResult result;
    auto session = Session::create(std::move(resources), profiles, output, server, shutdown, -1, result);
    assert(session && result == Session::CreateResult::Success);
    assert(!server.directMode());
    grab_changes.clear();

    for (int value : { 1, 2, 0 })
    {
      const input_event frame[]{ { {}, EV_KEY, KEY_SCROLLLOCK, value }, { {}, EV_SYN, SYN_REPORT, 0 } };
      assert(write(keyboard_pipe[1], frame, sizeof(frame)) == sizeof(frame));
    }
    // Both fds are ready. The loop drains the keyboard, then exits on mouse hangup.
    assert(close(mouse_pipe[1]) == 0);
    assert(session->run(shutdown) == Session::RunResult::MouseLost);
    assert(grab_changes.size() == 1);
    assert(grab_changes.front() == std::make_pair(1, LIBEVDEV_UNGRAB));
    assert(ipc.state_bits.load() & IPC_STATE_DIRECT_MODE);

    session.reset();
    assert(ipc.state_bits.load() == IPC_STATE_DIRECT_MODE);
    assert(close(keyboard_pipe[1]) == 0);
    keyboard_device = nullptr;

    SessionResources replacement;
    replacement.devices.mouse = InputDeviceHandle{ -1, libevdev_new(), {} };
    grab_changes.clear();
    session = Session::create(std::move(replacement), profiles, output, server, shutdown, -1, result);
    assert(session && result == Session::CreateResult::Success);
    assert(grab_changes.empty());
    assert((ipc.state_bits.load() & (IPC_STATE_CONNECTED | IPC_STATE_DIRECT_MODE)) ==
           (IPC_STATE_CONNECTED | IPC_STATE_DIRECT_MODE));
    ipc.requested_profile_id.store(1);
    assert(session->processIpcRequests());
    assert(ipc.active_profile_id.load() == 1);
    assert(server.directMode());
  }

  static void testRestartReuse(IpcServer& server, SmoothScrollIPC& ipc)
  {
    SessionConfig config;
    config.profiles.resize(2);
    for (auto& profile : config.profiles)
    {
      profile.smoother.mouse_movement_delay_microseconds = 0;
      profile.smoother.mouse_movement_window_milliseconds = 10000;
      profile.smoother.max_mouse_movement_distance = 30;
    }
    ProfileSet profile_set{ config };
    auto* original = profile_set.profiles.data();
    const auto* original_union = &profile_set.passthrough_union_keys;
    VirtualDevice output;
    {
      Session session{ {}, profile_set, output, server, -1 };
      for (ProfileId id = 0; id < profile_set.profiles.size(); ++id)
      {
        ipc.requested_profile_id.store(id);
        session.processIpcRequests();
        session.smoother().handleEvent(timeval{ 1, 0 }, true, false);
        MouseFrameState movement{ 20, 0 };
        assert(session.smoother().handleReportEvent(timeval{ 1, 1000 }, movement) == WheelSmoother::ReportResult::None);
      }
      // Teardown stops the active profile; switching already stopped the others.
      // Keyboard state is deliberately left for Session teardown.
      KeyboardPassthroughState keyboard;
      session.handleObservedKey(KEY_LEFTCTRL, 1, keyboard);
    }
    assert(profile_set.profiles.data() == original);
    assert(&profile_set.passthrough_union_keys == original_union);
    for (const auto& profile : profile_set.profiles)
    {
      assert(!profile.smoother.next_tick_time());
      assert(!profile.smoother.specialModeActive());
    }
    {
      Session session{ {}, profile_set, output, server, -1 };
      assert(session.active_profile_index_ == 0);
      assert(session.num_union_pressed_ == 0 && session.num_passthrough_ == 0);
      assert(session.events_.empty());
      for (ProfileId id = 0; id < profile_set.profiles.size(); ++id)
      {
        ipc.requested_profile_id.store(id);
        session.processIpcRequests();
        session.smoother().handleEvent(timeval{ 1, 2000 }, true, false);
        MouseFrameState movement{ 20, 0 };
        assert(session.smoother().handleReportEvent(timeval{ 1, 3000 }, movement) == WheelSmoother::ReportResult::None);
        assert(session.smoother().next_tick_time());
      }
      session.stopMotion();
    }
  }

  static void testInterruptedModes(IpcServer& server, SmoothScrollIPC& ipc)
  {
    SessionConfig config;
    config.profiles.resize(2);
    config.profiles[1].smoother.auto_scroll_activation_mode = WheelSmoother::AutoScrollActivationMode::Always;
    ProfileSet profile_set{ config };
    VirtualDevice output;
    for (int mode = 0; mode < 3; ++mode)
    {
      {
        Session session{ {}, profile_set, output, server, -1 };
        ipc.requested_profile_id.store(1);
        assert(session.processIpcRequests());
        session.smoother().handleEvent(timeval{ 1, 0 }, true, false);
        if (mode == 0)
          session.smoother().handleFreeSpinButton(1);
        else if (mode == 1)
          session.smoother().handleDragViewButton(timeval{ 1, 1000 }, 1);
        else
          session.smoother().handleAutoScrollButton(timeval{ 1, 1000 }, BTN_MIDDLE, 1);
        assert(session.smoother().specialModeActive());
        // Simulate device loss: no release event and no orderly stopMotion().
      }
      assert(!profile_set.profiles[1].smoother.specialModeActive());
      assert(!profile_set.profiles[1].smoother.next_tick_time());
      assert(profile_set.profiles[1].smoother.speed() == 0);
      {
        Session session{ {}, profile_set, output, server, -1 };
        assert(session.active_profile_index_ == 0);
        assert(session.processIpcRequests());
        session.smoother().handleEvent(timeval{ 2, 0 }, true, false);
        assert(session.smoother().next_tick_time());
        assert(!session.smoother().specialModeActive());
        ipc.requested_profile_id.store(0);
        assert(session.processIpcRequests());
      }
    }
  }

  static void run(IpcServer& server, SmoothScrollIPC& ipc)
  {
    SessionConfig config;
    config.profiles.resize(2);
    config.profiles[0].smoother.auto_scroll_activation_mode = WheelSmoother::AutoScrollActivationMode::Always;
    config.profiles[0].smoother.auto_scroll_latch_on_release = true;
    VirtualDevice output;
    ProfileSet profile_set{ config };
    Session session{ {}, profile_set, output, server, -1 };
    auto request = [&](ProfileId id) { ipc.requested_profile_id.store(id); };
    auto brake = [&] { ipc.scroll_id.fetch_add(1); };

    assert(!session.processIpcRequests());
    // Consume idle brakes before subsequent input starts a new scroll.
    brake();
    assert(session.processIpcRequests());
    session.smoother().handleEvent(timeval{ 1, 0 }, true, false);
    assert(session.smoother().next_tick_time());
    assert(!session.processIpcRequests());
    assert(session.smoother().next_tick_time());

    // Both requests are handled in one pass; the old due tick must be skipped.
    request(1);
    brake();
    assert(session.processIpcRequests());
    assert(ipc.active_profile_id.load() == 1);
    assert(!session.profile_set_.profiles[0].smoother.next_tick_time());
    assert(!session.processIpcRequests());
    request(0);
    assert(session.processIpcRequests());

    // A brake must not forcibly release a held special button.
    session.smoother().handleEvent(timeval{ 1, 10000 }, true, false);
    assert(session.smoother().handleFreeSpinButton(1));
    request(1);
    brake();
    assert(session.processIpcRequests());
    assert(ipc.active_profile_id.load() == 0);
    assert(session.smoother().free_spin());
    assert(!session.processIpcRequests());
    session.smoother().handleFreeSpinButton(0);
    assert(session.processIpcRequests());
    request(0);
    assert(session.processIpcRequests());

    // Braking a latched Auto Scroll makes switching eligible in the same pass.
    session.smoother().handleAutoScrollButton(timeval{ 2, 0 }, BTN_MIDDLE, 1);
    input_event movement{ timeval{ 2, 500 }, EV_REL, REL_Y, 100 };
    assert(!session.smoother().handleRelYEvent(movement, session.mouse_frame_));
    assert(session.smoother().handleReportEvent(movement.time, session.mouse_frame_) ==
           WheelSmoother::ReportResult::AutoScrollOffsetChanged);
    session.smoother().handleAutoScrollButton(timeval{ 2, 1000 }, BTN_MIDDLE, 0);
    assert(session.smoother().auto_scroll());
    request(1);
    assert(!session.processIpcRequests());
    brake();
    assert(session.processIpcRequests());
    assert(ipc.active_profile_id.load() == 1);
    assert(!session.profile_set_.profiles[0].smoother.auto_scroll());

    // Two keyboards holding the same union key must both release it.
    KeyboardPassthroughState first, second;
    session.handleObservedKey(KEY_LEFTCTRL, 1, first);
    session.handleObservedKey(KEY_LEFTCTRL, 1, second);
    request(0);
    brake();
    assert(session.processIpcRequests());
    assert(ipc.active_profile_id.load() == 1);
    session.handleObservedKey(KEY_LEFTCTRL, 0, first);
    assert(!session.processIpcRequests());
    session.handleObservedKey(KEY_LEFTCTRL, 0, second);
    assert(session.processIpcRequests());
    assert(ipc.active_profile_id.load() == 0);

    request(99);
    assert(!session.processIpcRequests());
    assert(ipc.active_profile_id.load() == 0);
  }
};

}  // namespace smooth_scroll

int main()
{
  using namespace smooth_scroll;
  const std::string name = "/ss_session_test_" + std::to_string(getpid());
  IpcServer server{ name };
  assert(server.initialize());
  const int fd = shm_open(name.c_str(), O_RDWR, 0);
  assert(fd >= 0);
  void* address = mmap(nullptr, sizeof(SmoothScrollIPC), PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
  assert(address != MAP_FAILED);
  SessionIpcTestAccess::run(server, *static_cast<SmoothScrollIPC*>(address));
  SessionIpcTestAccess::testRestartReuse(server, *static_cast<SmoothScrollIPC*>(address));
  SessionIpcTestAccess::testInterruptedModes(server, *static_cast<SmoothScrollIPC*>(address));
  SessionIpcTestAccess::testDirectModeRequests(server, *static_cast<SmoothScrollIPC*>(address));
  assert(munmap(address, sizeof(SmoothScrollIPC)) == 0);
  assert(close(fd) == 0);
  assert(shm_unlink(name.c_str()) == 0);
  std::cout << "session_ipc_test: all tests passed\n";
}
