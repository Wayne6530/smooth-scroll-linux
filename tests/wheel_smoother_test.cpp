// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Wayne6530

#include "wheel_smoother.h"

#ifdef NDEBUG
#undef NDEBUG
#endif
#include <cassert>
#include <cmath>
#include <iostream>

using smooth_scroll::WheelSmoother;

namespace
{

timeval atMilliseconds(int64_t milliseconds)
{
  return { milliseconds / 1000, (milliseconds % 1000) * 1000 };
}

input_event relativeEvent(int code, int value, int64_t milliseconds)
{
  return { atMilliseconds(milliseconds), EV_REL, static_cast<__u16>(code), value };
}

WheelSmoother::Options autoOptions()
{
  WheelSmoother::Options options;
  options.auto_scroll_activation_mode = WheelSmoother::AutoScrollActivationMode::Always;
  options.auto_scroll_axis_mode = WheelSmoother::AutoScrollAxisMode::Omnidirectional;
  options.auto_scroll_deadzone = 8;
  options.auto_scroll_click_timeout_milliseconds = 200;
  options.auto_scroll_speed_factor = 50;
  options.auto_scroll_max_speed = 6000;
  return options;
}

void move(WheelSmoother& smoother, int x, int y, int64_t milliseconds)
{
  if (x != 0)
  {
    auto ev = relativeEvent(REL_X, x, milliseconds);
    assert(!smoother.handleRelXEvent(ev));
  }
  if (y != 0)
  {
    auto ev = relativeEvent(REL_Y, y, milliseconds);
    assert(!smoother.handleRelYEvent(ev));
  }
  static_cast<void>(smoother.handleReportEvent(atMilliseconds(milliseconds)));
}

void testReportBuffersAutoScrollOffset()
{
  WheelSmoother smoother{ autoOptions() };
  smoother.handleAutoScrollButton(atMilliseconds(0), BTN_MIDDLE, 1);

  auto x1 = relativeEvent(REL_X, 5, 1);
  auto x2 = relativeEvent(REL_X, 7, 1);
  auto y = relativeEvent(REL_Y, -3, 1);
  assert(!smoother.handleRelXEvent(x1));
  assert(!smoother.handleRelXEvent(x2));
  assert(!smoother.handleRelYEvent(y));
  assert(smoother.auto_scroll_offset_x() == 0);
  assert(smoother.auto_scroll_offset_y() == 0);

  assert(smoother.handleReportEvent(atMilliseconds(1)) ==
         WheelSmoother::ReportResult::AutoScrollOffsetChanged);
  assert(smoother.auto_scroll_offset_x() == 12);
  assert(smoother.auto_scroll_offset_y() == -3);
  assert(smoother.handleReportEvent(atMilliseconds(1)) == WheelSmoother::ReportResult::None);

  auto x3 = relativeEvent(REL_X, 9, 2);
  assert(!smoother.handleRelXEvent(x3));
  assert(smoother.handleAutoScrollButton(atMilliseconds(2), BTN_MIDDLE, 0) ==
         WheelSmoother::ButtonResult::Handled);
  assert(smoother.auto_scroll_offset_x() == 12);
  assert(smoother.auto_scroll());
  assert(smoother.handleReportEvent(atMilliseconds(2)) ==
         WheelSmoother::ReportResult::AutoScrollOffsetChanged);
  assert(smoother.auto_scroll_offset_x() == 21);
}

void testOnlyWhileScrollingEligibility()
{
  WheelSmoother smoother{ WheelSmoother::Options{} };

  assert(smoother.handleAutoScrollButton(atMilliseconds(0), BTN_MIDDLE, 1) ==
         WheelSmoother::ButtonResult::Passthrough);
  auto ev = relativeEvent(REL_X, 20, 1);
  assert(smoother.handleRelXEvent(ev));
  assert(smoother.handleAutoScrollButton(atMilliseconds(100), BTN_MIDDLE, 0) ==
         WheelSmoother::ButtonResult::Passthrough);

  smoother.handleEvent(atMilliseconds(200), true, false);
  assert(smoother.speed() > 0);
  assert(smoother.handleAutoScrollButton(atMilliseconds(201), BTN_MIDDLE, 1) ==
         WheelSmoother::ButtonResult::Handled);
  assert(smoother.speed() == 0);
  assert(smoother.auto_scroll());
}

void testClickPreservation()
{
  WheelSmoother smoother{ autoOptions() };
  assert(smoother.handleAutoScrollButton(atMilliseconds(0), BTN_MIDDLE, 1) ==
         WheelSmoother::ButtonResult::Handled);
  assert(smoother.auto_scroll());
  move(smoother, 4, 4, 20);
  assert(smoother.auto_scroll());
  assert(smoother.handleAutoScrollButton(atMilliseconds(100), BTN_MIDDLE, 0) ==
         WheelSmoother::ButtonResult::ReplayClick);
  assert(!smoother.auto_scroll());

  assert(smoother.handleAutoScrollButton(atMilliseconds(200), BTN_MIDDLE, 1) ==
         WheelSmoother::ButtonResult::Handled);
  move(smoother, 20, 0, 210);
  move(smoother, -20, 0, 220);
  assert(smoother.handleAutoScrollButton(atMilliseconds(250), BTN_MIDDLE, 0) ==
         WheelSmoother::ButtonResult::ReplayClick);

  assert(smoother.handleAutoScrollButton(atMilliseconds(300), BTN_MIDDLE, 1) ==
         WheelSmoother::ButtonResult::Handled);
  assert(smoother.handleAutoScrollButton(atMilliseconds(550), BTN_MIDDLE, 0) ==
         WheelSmoother::ButtonResult::Handled);
}

void testOmnidirectionalLatchReverseAndExit()
{
  WheelSmoother smoother{ autoOptions() };
  smoother.handleAutoScrollButton(atMilliseconds(0), BTN_MIDDLE, 1);
  move(smoother, 12, 12, 1);
  assert(smoother.auto_scroll());
  assert(smoother.auto_scroll_offset_x() == 12);
  assert(smoother.auto_scroll_offset_y() == 12);
  assert(smoother.speed() == 0);

  assert(smoother.handleAutoScrollButton(atMilliseconds(10), BTN_MIDDLE, 0) ==
         WheelSmoother::ButtonResult::Handled);
  assert(smoother.auto_scroll());

  move(smoother, -24, -24, 11);
  assert(smoother.auto_scroll_offset_x() == -12);
  assert(smoother.auto_scroll_offset_y() == -12);

  assert(smoother.handleAutoScrollButton(atMilliseconds(20), BTN_MIDDLE, 1) ==
         WheelSmoother::ButtonResult::Handled);
  move(smoother, 0, 5, 21);
  assert(smoother.auto_scroll());
  assert(smoother.handleAutoScrollButton(atMilliseconds(30), BTN_MIDDLE, 0) ==
         WheelSmoother::ButtonResult::Handled);
  assert(!smoother.auto_scroll());
  assert(smoother.speed() == 0);
}

void testAxisModesAndDualAxisOutput()
{
  auto vertical_options = autoOptions();
  vertical_options.auto_scroll_axis_mode = WheelSmoother::AutoScrollAxisMode::Vertical;
  WheelSmoother vertical{ vertical_options };
  vertical.handleAutoScrollButton(atMilliseconds(0), BTN_MIDDLE, 1);
  move(vertical, 20, -20, 1);
  assert(vertical.auto_scroll_offset_x() == 0);
  assert(vertical.auto_scroll_offset_y() == -20);
  assert(!vertical.auto_scroll_horizontal_enabled());
  assert(vertical.auto_scroll_vertical_enabled());
  assert(vertical.speed() == 0);
  const auto vertical_result = vertical.tick();
  assert(vertical_result.count == 1);
  assert(vertical_result.events[0].code == REL_WHEEL_HI_RES);
  assert(vertical_result.events[0].value == 1);

  auto horizontal_options = autoOptions();
  horizontal_options.auto_scroll_axis_mode = WheelSmoother::AutoScrollAxisMode::Horizontal;
  WheelSmoother horizontal{ horizontal_options };
  horizontal.handleAutoScrollButton(atMilliseconds(0), BTN_MIDDLE, 1);
  move(horizontal, -20, -20, 1);
  assert(horizontal.auto_scroll_offset_x() == -20);
  assert(horizontal.auto_scroll_offset_y() == 0);
  assert(horizontal.auto_scroll_horizontal_enabled());
  assert(!horizontal.auto_scroll_vertical_enabled());
  assert(horizontal.speed() == 0);
  const auto horizontal_result = horizontal.tick();
  assert(horizontal_result.count == 1);
  assert(horizontal_result.events[0].code == REL_HWHEEL_HI_RES);
  assert(horizontal_result.events[0].value == -1);

  auto omni_options = autoOptions();
  omni_options.tick_interval_microseconds = 100000;
  omni_options.auto_scroll_deadzone = 0;
  omni_options.auto_scroll_speed_factor = 10;
  WheelSmoother omni{ omni_options };
  omni.handleAutoScrollButton(atMilliseconds(0), BTN_MIDDLE, 1);
  move(omni, 10, -20, 1);

  const auto result = omni.tick();
  assert(result.count == 2);
  assert(result.events[0].code == REL_WHEEL_HI_RES);
  assert(result.events[0].value == 20);
  assert(result.events[1].code == REL_HWHEEL_HI_RES);
  assert(result.events[1].value == 10);
  assert(result.events[0].time.tv_sec == result.events[1].time.tv_sec);
  assert(result.events[0].time.tv_usec == result.events[1].time.tv_usec);
}

void testFractionalOutput()
{
  auto options = autoOptions();
  options.tick_interval_microseconds = 125000;
  options.auto_scroll_speed_factor = 1;
  WheelSmoother smoother{ options };

  smoother.handleAutoScrollButton(atMilliseconds(0), BTN_MIDDLE, 1);
  move(smoother, 9, 0, 1);
  assert(smoother.speed() == 0);

  int output = 0;
  for (int i = 0; i < 16; ++i)
  {
    const auto result = smoother.tick();
    for (std::size_t j = 0; j < result.count; ++j)
    {
      output += result.events[j].value;
    }
  }
  assert(output == 2);
}

void testAutoScrollOffsetSaturation()
{
  auto options = autoOptions();
  options.tick_interval_microseconds = 1'000'000;
  options.auto_scroll_speed_factor = 30;
  options.auto_scroll_max_speed = 100;
  WheelSmoother smoother{ options };

  smoother.handleAutoScrollButton(atMilliseconds(0), BTN_MIDDLE, 1);
  move(smoother, 1000, -1000, 1);

  // deadzone + ceil(max speed / speed factor) = 8 + ceil(100 / 30) = 12
  assert(smoother.auto_scroll_offset_x() == 12);
  assert(smoother.auto_scroll_offset_y() == -12);
  auto result = smoother.tick();
  assert(result.count == 2);
  assert(result.events[0].value == 100);
  assert(result.events[1].value == 100);

  move(smoother, -1, 1, 2);
  assert(smoother.auto_scroll_offset_x() == 11);
  assert(smoother.auto_scroll_offset_y() == -11);
  result = smoother.tick();
  assert(result.count == 2);
  assert(result.events[0].value == 90);
  assert(result.events[1].value == 90);

  move(smoother, -1000, 1000, 3);
  assert(smoother.auto_scroll_offset_x() == -12);
  assert(smoother.auto_scroll_offset_y() == 12);
}

void testButtonsAndBraking()
{
  auto options = autoOptions();
  options.drag_view_activation_mode = WheelSmoother::DragViewActivationMode::Scrolling;
  WheelSmoother smoother{ options };

  smoother.handleAutoScrollButton(atMilliseconds(0), BTN_MIDDLE, 1);
  assert(smoother.handleOrdinaryButton(BTN_LEFT, 1) == WheelSmoother::ButtonResult::Passthrough);
  assert(smoother.auto_scroll());
  move(smoother, 20, 0, 1);
  assert(smoother.auto_scroll());

  smoother.stop();
  assert(smoother.auto_scroll());
  assert(smoother.speed() == 0);
  move(smoother, -20, 0, 2);

  assert(smoother.handleDragViewButton(atMilliseconds(3), 1) ==
         WheelSmoother::ButtonResult::Passthrough);
  assert(!smoother.drag_view());

  assert(smoother.handleFreeSpinButton(1));
  assert(smoother.free_spin());
  assert(smoother.auto_scroll());
  assert(smoother.handleFreeSpinButton(0));

  smoother.handleAutoScrollButton(atMilliseconds(4), BTN_MIDDLE, 0);
  assert(smoother.auto_scroll());
  smoother.stop();
  assert(!smoother.auto_scroll());

  smoother.handleAutoScrollButton(atMilliseconds(10), BTN_MIDDLE, 1);
  move(smoother, 20, 0, 11);
  smoother.handleAutoScrollButton(atMilliseconds(12), BTN_MIDDLE, 0);
  assert(smoother.handleOrdinaryButton(BTN_LEFT, 1) == WheelSmoother::ButtonResult::Handled);
  assert(smoother.auto_scroll());
  assert(smoother.handleOrdinaryButton(BTN_LEFT, 0) == WheelSmoother::ButtonResult::Handled);
  assert(!smoother.auto_scroll());

  smoother.handleAutoScrollButton(atMilliseconds(20), BTN_MIDDLE, 1);
  move(smoother, 20, 0, 21);
  smoother.handleAutoScrollButton(atMilliseconds(22), BTN_MIDDLE, 0);
  assert(smoother.handleDragViewButton(atMilliseconds(23), 1) ==
         WheelSmoother::ButtonResult::Handled);
  assert(smoother.drag_view());
  assert(!smoother.auto_scroll());
}

void testLatchedWheelActions()
{
  WheelSmoother ignored{ autoOptions() };
  ignored.handleAutoScrollButton(atMilliseconds(0), BTN_MIDDLE, 1);
  move(ignored, 20, 0, 1);
  ignored.handleAutoScrollButton(atMilliseconds(2), BTN_MIDDLE, 0);
  assert(!ignored.handleEvent(atMilliseconds(3), true, false).has_value());
  assert(ignored.auto_scroll());

  auto exit_options = autoOptions();
  exit_options.auto_scroll_wheel_action = WheelSmoother::AutoScrollWheelAction::Exit;
  WheelSmoother exited{ exit_options };
  exited.handleAutoScrollButton(atMilliseconds(10), BTN_MIDDLE, 1);
  move(exited, 20, 0, 11);
  exited.handleAutoScrollButton(atMilliseconds(12), BTN_MIDDLE, 0);
  assert(!exited.handleEvent(atMilliseconds(13), true, false).has_value());
  assert(!exited.auto_scroll());
  assert(exited.auto_scroll_offset_x() == 0);
  assert(exited.auto_scroll_offset_y() == 0);

  static_cast<void>(exited.handleEvent(atMilliseconds(14), true, false));
  assert(exited.speed() > 0);

  WheelSmoother held{ exit_options };
  held.handleAutoScrollButton(atMilliseconds(20), BTN_MIDDLE, 1);
  move(held, 20, 0, 21);
  assert(!held.handleEvent(atMilliseconds(22), true, false).has_value());
  assert(held.auto_scroll());
  held.handleAutoScrollButton(atMilliseconds(23), BTN_MIDDLE, 0);
  held.handleAutoScrollButton(atMilliseconds(24), BTN_MIDDLE, 1);
  assert(!held.handleEvent(atMilliseconds(25), true, false).has_value());
  assert(held.auto_scroll());
  held.handleAutoScrollButton(atMilliseconds(26), BTN_MIDDLE, 0);
  assert(!held.auto_scroll());
}

void testAnyButtonHeldExit()
{
  auto options = autoOptions();
  options.auto_scroll_exit_button_mode = WheelSmoother::AutoScrollExitButtonMode::AnyButton;
  WheelSmoother smoother{ options };

  smoother.handleAutoScrollButton(atMilliseconds(0), BTN_MIDDLE, 1);
  move(smoother, 20, 0, 1);
  smoother.handleAutoScrollButton(atMilliseconds(2), BTN_MIDDLE, 0);

  assert(smoother.handleOrdinaryButton(BTN_RIGHT, 0) == WheelSmoother::ButtonResult::Passthrough);
  assert(!smoother.auto_scroll());

  smoother.handleAutoScrollButton(atMilliseconds(10), BTN_MIDDLE, 1);
  move(smoother, 20, 0, 11);
  smoother.handleAutoScrollButton(atMilliseconds(12), BTN_MIDDLE, 0);
  assert(smoother.auto_scroll());
  assert(smoother.handleOrdinaryButton(BTN_LEFT, 1) == WheelSmoother::ButtonResult::Handled);
  assert(smoother.auto_scroll());

  assert(smoother.handleOrdinaryButton(BTN_RIGHT, 1) == WheelSmoother::ButtonResult::Passthrough);
  assert(smoother.handleOrdinaryButton(BTN_RIGHT, 0) == WheelSmoother::ButtonResult::Passthrough);
  assert(smoother.handleAutoScrollButton(atMilliseconds(13), BTN_MIDDLE, 1) ==
         WheelSmoother::ButtonResult::Passthrough);
  assert(smoother.handleDragViewButton(atMilliseconds(14), 1) ==
         WheelSmoother::ButtonResult::Passthrough);
  assert(smoother.handleFreeSpinButton(1));
  assert(smoother.handleFreeSpinButton(0));
  assert(smoother.auto_scroll());

  assert(smoother.handleOrdinaryButton(BTN_LEFT, 2) == WheelSmoother::ButtonResult::Handled);
  assert(smoother.handleOrdinaryButton(BTN_LEFT, 0) == WheelSmoother::ButtonResult::Handled);
  assert(!smoother.auto_scroll());

  auto restricted_options = autoOptions();
  restricted_options.auto_scroll_exit_button_mode = WheelSmoother::AutoScrollExitButtonMode::AutoScrollButton;
  WheelSmoother restricted{ restricted_options };
  restricted.handleAutoScrollButton(atMilliseconds(20), BTN_MIDDLE, 1);
  move(restricted, 20, 0, 21);
  restricted.handleAutoScrollButton(atMilliseconds(22), BTN_MIDDLE, 0);
  assert(restricted.handleOrdinaryButton(BTN_LEFT, 1) == WheelSmoother::ButtonResult::Passthrough);
  assert(!restricted.auto_scroll());
}

void testModeButtonOrdinaryFallback()
{
  WheelSmoother smoother{ WheelSmoother::Options{} };

  static_cast<void>(smoother.handleEvent(atMilliseconds(0), true, false));
  assert(smoother.speed() > 0);
  assert(smoother.handleAutoScrollButton(atMilliseconds(1), BTN_MIDDLE, 0) ==
         WheelSmoother::ButtonResult::Passthrough);
  assert(smoother.speed() == 0);

  static_cast<void>(smoother.handleEvent(atMilliseconds(2), true, false));
  assert(smoother.speed() > 0);
  assert(smoother.handleDragViewButton(atMilliseconds(3), 0) ==
         WheelSmoother::ButtonResult::Passthrough);
  assert(smoother.speed() == 0);

  WheelSmoother latched{ autoOptions() };
  latched.handleAutoScrollButton(atMilliseconds(10), BTN_MIDDLE, 1);
  move(latched, 20, 0, 11);
  latched.handleAutoScrollButton(atMilliseconds(12), BTN_MIDDLE, 0);
  assert(latched.auto_scroll());
  assert(latched.handleAutoScrollButton(atMilliseconds(13), BTN_MIDDLE, 0) ==
         WheelSmoother::ButtonResult::Passthrough);
  assert(!latched.auto_scroll());

  auto any_options = autoOptions();
  any_options.auto_scroll_exit_button_mode = WheelSmoother::AutoScrollExitButtonMode::AnyButton;

  WheelSmoother any_auto_button{ any_options };
  any_auto_button.handleAutoScrollButton(atMilliseconds(20), BTN_MIDDLE, 1);
  move(any_auto_button, 20, 0, 21);
  any_auto_button.handleAutoScrollButton(atMilliseconds(22), BTN_MIDDLE, 0);
  assert(any_auto_button.handleAutoScrollButton(atMilliseconds(23), BTN_MIDDLE, 0) ==
         WheelSmoother::ButtonResult::Passthrough);
  assert(!any_auto_button.auto_scroll());

  WheelSmoother any_drag_button{ any_options };
  any_drag_button.handleAutoScrollButton(atMilliseconds(30), BTN_MIDDLE, 1);
  move(any_drag_button, 20, 0, 31);
  any_drag_button.handleAutoScrollButton(atMilliseconds(32), BTN_MIDDLE, 0);
  assert(any_drag_button.handleDragViewButton(atMilliseconds(33), 0) ==
         WheelSmoother::ButtonResult::Passthrough);
  assert(!any_drag_button.auto_scroll());
}

void testPreActivationBrakeAndWheelIsolation()
{
  WheelSmoother smoother{ autoOptions() };
  smoother.handleAutoScrollButton(atMilliseconds(0), BTN_MIDDLE, 1);
  move(smoother, 5, 0, 1);
  assert(smoother.auto_scroll());
  assert(smoother.handleFreeSpinButton(1));
  assert(smoother.handleFreeSpinButton(0));
  assert(!smoother.handleEvent(atMilliseconds(2), true, false).has_value());

  smoother.stop();
  move(smoother, 5, 0, 3);
  assert(smoother.auto_scroll());
  move(smoother, 4, 0, 4);
  assert(smoother.auto_scroll());
}

void testDragOwnershipAndHardReset()
{
  auto options = autoOptions();
  options.drag_view_activation_mode = WheelSmoother::DragViewActivationMode::Always;
  WheelSmoother smoother{ options };

  assert(smoother.handleDragViewButton(atMilliseconds(0), 1) ==
         WheelSmoother::ButtonResult::Handled);
  smoother.stop();
  assert(smoother.drag_view());
  assert(smoother.handleAutoScrollButton(atMilliseconds(1), BTN_MIDDLE, 1) ==
         WheelSmoother::ButtonResult::Passthrough);
  assert(smoother.handleAutoScrollButton(atMilliseconds(2), BTN_MIDDLE, 0) ==
         WheelSmoother::ButtonResult::Passthrough);
  smoother.handleDragViewButton(atMilliseconds(3), 0);

  smoother.handleAutoScrollButton(atMilliseconds(10), BTN_MIDDLE, 1);
  move(smoother, 20, 0, 11);
  smoother.handleFreeSpinButton(1);
  smoother.hardReset();
  assert(!smoother.auto_scroll());
  assert(!smoother.drag_view());
  assert(!smoother.free_spin());
  assert(smoother.speed() == 0);
}

void testDragViewFractionalOutput()
{
  auto options = autoOptions();
  options.drag_view_activation_mode = WheelSmoother::DragViewActivationMode::Always;
  options.drag_view_speed = 0.25;
  WheelSmoother smoother{ options };

  assert(smoother.handleDragViewButton(atMilliseconds(0), 1) ==
         WheelSmoother::ButtonResult::Handled);

  for (int i = 0; i < 3; ++i)
  {
    auto ev = relativeEvent(REL_X, 1, i + 1);
    assert(!smoother.handleRelXEvent(ev));
  }
  auto x = relativeEvent(REL_X, 1, 4);
  assert(smoother.handleRelXEvent(x));
  assert(x.code == REL_HWHEEL_HI_RES);
  assert(x.value == 1);

  auto y1 = relativeEvent(REL_Y, 2, 5);
  assert(!smoother.handleRelYEvent(y1));
  auto y2 = relativeEvent(REL_Y, 2, 6);
  assert(smoother.handleRelYEvent(y2));
  assert(y2.code == REL_WHEEL_HI_RES);
  assert(y2.value == -1);

  smoother.handleDragViewButton(atMilliseconds(7), 0);
  assert(smoother.handleDragViewButton(atMilliseconds(8), 1) ==
         WheelSmoother::ButtonResult::Handled);
  auto reset_x = relativeEvent(REL_X, 1, 9);
  assert(!smoother.handleRelXEvent(reset_x));

  assert(smoother.handleDragViewButton(atMilliseconds(10), 1) ==
         WheelSmoother::ButtonResult::Handled);
  auto repeated_press_x = relativeEvent(REL_X, 3, 11);
  assert(smoother.handleRelXEvent(repeated_press_x));
  assert(repeated_press_x.value == 1);

  smoother.hardReset();
  assert(!smoother.drag_view());

  options.drag_view_speed = -0.5;
  WheelSmoother reversed{ options };
  assert(reversed.handleDragViewButton(atMilliseconds(20), 1) ==
         WheelSmoother::ButtonResult::Handled);
  auto reversed_x1 = relativeEvent(REL_X, 1, 21);
  assert(!reversed.handleRelXEvent(reversed_x1));
  auto reversed_x2 = relativeEvent(REL_X, 1, 22);
  assert(reversed.handleRelXEvent(reversed_x2));
  assert(reversed_x2.value == -1);
}

void testSpecialModeActive()
{
  WheelSmoother free_spin{ WheelSmoother::Options{} };
  assert(!free_spin.specialModeActive());
  static_cast<void>(free_spin.handleEvent(atMilliseconds(0), true, false));
  assert(free_spin.handleFreeSpinButton(1));
  assert(free_spin.specialModeActive());
  free_spin.stop();
  assert(free_spin.specialModeActive());
  assert(free_spin.handleFreeSpinButton(0));
  assert(!free_spin.specialModeActive());

  auto drag_options = autoOptions();
  drag_options.drag_view_activation_mode = WheelSmoother::DragViewActivationMode::Always;
  WheelSmoother drag_view{ drag_options };
  assert(drag_view.handleDragViewButton(atMilliseconds(0), 1) == WheelSmoother::ButtonResult::Handled);
  assert(drag_view.specialModeActive());
  assert(drag_view.handleDragViewButton(atMilliseconds(1), 0) == WheelSmoother::ButtonResult::ReplayClick);
  assert(!drag_view.specialModeActive());

  WheelSmoother auto_scroll{ autoOptions() };
  assert(auto_scroll.handleAutoScrollButton(atMilliseconds(0), BTN_MIDDLE, 1) ==
         WheelSmoother::ButtonResult::Handled);
  assert(auto_scroll.specialModeActive());
  assert(auto_scroll.handleAutoScrollButton(atMilliseconds(1), BTN_MIDDLE, 0) ==
         WheelSmoother::ButtonResult::ReplayClick);
  assert(!auto_scroll.specialModeActive());
}

void testProcessingMode()
{
  WheelSmoother smoother{ WheelSmoother::Options{} };
  static_cast<void>(smoother.handleEvent(atMilliseconds(0), true, false));
  assert(smoother.speed() > 0);
  assert(smoother.prepareProcessing(false) == WheelSmoother::ProcessingMode::Process);
  assert(smoother.speed() > 0);
  assert(smoother.prepareProcessing(true) == WheelSmoother::ProcessingMode::CompatibilityPassthrough);
  assert(smoother.speed() == 0);
  assert(!smoother.next_tick_time());
  assert(smoother.prepareProcessing(true) == WheelSmoother::ProcessingMode::CompatibilityPassthrough);

  WheelSmoother free_spin{ WheelSmoother::Options{} };
  static_cast<void>(free_spin.handleEvent(atMilliseconds(0), true, false));
  assert(free_spin.handleFreeSpinButton(1));
  assert(free_spin.prepareProcessing(true) == WheelSmoother::ProcessingMode::Process);
  assert(free_spin.free_spin());
  assert(free_spin.speed() > 0);
  assert(free_spin.next_tick_time());
  assert(free_spin.handleFreeSpinButton(0));
  assert(free_spin.prepareProcessing(true) == WheelSmoother::ProcessingMode::CompatibilityPassthrough);

  auto drag_options = autoOptions();
  drag_options.drag_view_activation_mode = WheelSmoother::DragViewActivationMode::Always;
  WheelSmoother drag_view{ drag_options };
  assert(drag_view.handleDragViewButton(atMilliseconds(0), 1) == WheelSmoother::ButtonResult::Handled);
  assert(drag_view.prepareProcessing(true) == WheelSmoother::ProcessingMode::Process);
  assert(drag_view.handleDragViewButton(atMilliseconds(1), 0) == WheelSmoother::ButtonResult::ReplayClick);
  assert(drag_view.prepareProcessing(true) == WheelSmoother::ProcessingMode::CompatibilityPassthrough);

  WheelSmoother auto_scroll{ autoOptions() };
  assert(auto_scroll.handleAutoScrollButton(atMilliseconds(0), BTN_MIDDLE, 1) ==
         WheelSmoother::ButtonResult::Handled);
  move(auto_scroll, 12, 0, 1);
  assert(auto_scroll.handleAutoScrollButton(atMilliseconds(2), BTN_MIDDLE, 0) ==
         WheelSmoother::ButtonResult::Handled);
  assert(auto_scroll.prepareProcessing(true) == WheelSmoother::ProcessingMode::Process);
  assert(auto_scroll.auto_scroll());
  assert(auto_scroll.auto_scroll_offset_x() == 12);
  assert(auto_scroll.handleAutoScrollButton(atMilliseconds(3), BTN_MIDDLE, 1) ==
         WheelSmoother::ButtonResult::Handled);
  assert(auto_scroll.prepareProcessing(true) == WheelSmoother::ProcessingMode::Process);
  assert(auto_scroll.handleAutoScrollButton(atMilliseconds(4), BTN_MIDDLE, 0) ==
         WheelSmoother::ButtonResult::Handled);
  assert(auto_scroll.prepareProcessing(true) == WheelSmoother::ProcessingMode::CompatibilityPassthrough);
}

}  // namespace

int main()
{
  testReportBuffersAutoScrollOffset();
  testOnlyWhileScrollingEligibility();
  testClickPreservation();
  testOmnidirectionalLatchReverseAndExit();
  testAxisModesAndDualAxisOutput();
  testFractionalOutput();
  testAutoScrollOffsetSaturation();
  testButtonsAndBraking();
  testPreActivationBrakeAndWheelIsolation();
  testDragOwnershipAndHardReset();
  testDragViewFractionalOutput();
  testLatchedWheelActions();
  testAnyButtonHeldExit();
  testModeButtonOrdinaryFallback();
  testSpecialModeActive();
  testProcessingMode();
  std::cout << "wheel_smoother_test: all tests passed\n";
  return 0;
}
