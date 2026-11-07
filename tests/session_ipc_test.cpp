// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Wayne6530

#include "session.h"

#ifdef NDEBUG
#undef NDEBUG
#endif
#include <cassert>
#include <iostream>
#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>

namespace smooth_scroll
{

// Exercise the real request handler without acquiring physical input devices.
struct SessionIpcTestAccess
{
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
        assert(session.smoother().handleReportEvent(timeval{ 1, 1000 }, movement) ==
               WheelSmoother::ReportResult::None);
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
        assert(session.smoother().handleReportEvent(timeval{ 1, 3000 }, movement) ==
               WheelSmoother::ReportResult::None);
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
  assert(munmap(address, sizeof(SmoothScrollIPC)) == 0);
  assert(close(fd) == 0);
  assert(shm_unlink(name.c_str()) == 0);
  std::cout << "session_ipc_test: all tests passed\n";
}
