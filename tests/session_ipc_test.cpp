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
  static void run(IpcServer& server, SmoothScrollIPC& ipc)
  {
    SessionConfig config;
    config.profiles.resize(2);
    config.profiles[0].smoother.auto_scroll_activation_mode = WheelSmoother::AutoScrollActivationMode::Always;
    config.profiles[0].smoother.auto_scroll_latch_on_release = true;
    VirtualDevice output;
    Session session{ {}, config, output, server, -1 };
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
    assert(!session.profiles_[0].smoother.next_tick_time());
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
    assert(!session.profiles_[0].smoother.auto_scroll());

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
  assert(munmap(address, sizeof(SmoothScrollIPC)) == 0);
  assert(close(fd) == 0);
  assert(shm_unlink(name.c_str()) == 0);
  std::cout << "session_ipc_test: all tests passed\n";
}
