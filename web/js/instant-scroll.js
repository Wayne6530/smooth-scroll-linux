/** Web-only preset: existing Distance physics consumes the budget on the first event. */
const InstantScroll = (() => {
  const managedKeys = [
    'smooth_mode', 'damping', 'min_deceleration', 'max_deceleration',
    'free_spin_button', 'drag_view_activation_mode', 'auto_scroll_activation_mode',
  ];
  const disabledKeys = new Set([
    ...managedKeys,
    'initial_speed', 'speed_factor', 'speed_smooth_window_microseconds',
    'max_speed_change_lowerbound', 'min_speed_change_upperbound',
    'min_speed_change_ratio', 'max_speed_change_ratio',
    'use_reverse_scroll_braking', 'max_reverse_scroll_braking_microseconds',
    'max_reverse_scroll_braking_times', 'reverse_scroll_intent_window_microseconds',
    'use_mouse_movement_braking', 'max_mouse_movement_distance',
    'mouse_movement_window_milliseconds', 'mouse_movement_delay_microseconds',
  ]);

  function apply(values) {
    const dt = values.tick_interval_microseconds / 1e6;
    // 1% margin avoids a floating-point remainder. Damping stays positive and
    // the low-distance branch applies throughout the editor's distance/tick ranges.
    const deceleration = Math.ceil(1.01 * values.wheel_tick_distance / (2 * dt * dt));
    Object.assign(values, {
      smooth_mode: 1, damping: 3.1,
      min_deceleration: deceleration, max_deceleration: deceleration,
      free_spin_button: 0, drag_view_activation_mode: 1, auto_scroll_activation_mode: 1,
    });
  }

  function matches(values) {
    const distance = Number(values.wheel_tick_distance);
    const dt = Number(values.tick_interval_microseconds) / 1e6;
    const min = Number(values.min_deceleration);
    const max = Number(values.max_deceleration);
    const damping = Number(values.damping);
    if (Number(values.smooth_mode) !== 1 ||
        ![distance, dt, min, max, damping].every(Number.isFinite) ||
        distance <= 0 || dt <= 0 || min <= 0 || max < min || damping < 0) return false;
    // Same distance-to-speed curve as WheelSmoother, including zero damping.
    const lowSpeed = min / damping;
    const lowDistance = lowSpeed * lowSpeed / (2 * min);
    const highDistance = lowDistance + (max - min) / (damping * damping);
    const speed = distance <= lowDistance ? Math.sqrt(2 * min * distance)
      : distance <= highDistance ? lowSpeed + damping * (distance - lowDistance)
      : Math.sqrt(max * max / (damping * damping) + 2 * max * (distance - highDistance));
    return speed * dt >= distance;
  }

  return { managedKeys, disabledKeys, apply, matches };
})();
