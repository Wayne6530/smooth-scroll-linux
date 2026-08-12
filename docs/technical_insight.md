# Technical Insight

## Event Processing Pipeline  

The tool leverages Linux's input subsystem to achieve smooth scrolling through the following pipeline:  

1. **Device Acquisition**:  
   - Opens and exclusively locks a physical mouse device file (e.g., `/dev/input/event*`) to intercept raw input events.  

2. **Event Filtering**:  
   - **Discarded Events**:  
     - Raw high-resolution wheel events (`REL_WHEEL_HI_RES`) are **dropped immediately** to prevent interference.  
   - **Intercepted Events**:  
     - Standard wheel events (`REL_WHEEL`) are captured and forwarded to the **smoothing module**.  

3. **Smoothing Module**:  
   - Applies physics-based algorithms (inertia, damping) to transform discrete `REL_WHEEL` events into continuous motion.  
   - Generates synthetic high-resolution events (`REL_WHEEL_HI_RES`) for fluid scrolling.  

4. **Virtual Device Output**:  
   - Uses `uinput` to create a **virtual mouse device**.  
   - Merges smoothed `REL_WHEEL_HI_RES` events with other unmodified mouse events (e.g., clicks, movement) and emits them through the virtual device.  

## Smoothing Algorithm  

The physics-based smoothing algorithm transforms discrete wheel events into fluid motion using the following principles:

### Smoothing Modes

The `smooth_mode` option selects one of three models:

| Value | Mode | Behavior |
| ----- | ---- | -------- |
| `0` | Speed | Adapts speed and total travel distance to the timing of incoming wheel events. |
| `1` | Distance | Adds a fixed `wheel_tick_distance` budget for every accepted wheel event and releases that budget smoothly. |
| `2` | Hybrid | Runs the Speed model while using the Distance model as a minimum travel-distance budget. |

The smoother keeps the two model states separate:

- `delta_` is the current per-tick displacement produced by the Speed model. It is used only by Speed and Hybrid modes.
- `distance_remaining_` is the distance budget that has not yet been emitted. It is shared by Distance and Hybrid modes.

A scroll is active whenever either state is non-zero:

```text
scroll_active = delta_ != 0 || distance_remaining_ > 0
```

An explicit stop, changing axes, or applying a brake clears both states. Natural Speed-model decay can reach zero while Hybrid mode continues releasing a remaining distance budget.

### Speed Model

The speed is measured in `REL_WHEEL_HI_RES` values per second.  

1. **Initial Trigger**:  
   - When scrolling starts (after a stop), the first wheel event sets the initial speed (`initial_speed`).  

2. **Subsequent Events**:  
   - For each new wheel event in the **same direction**, the speed is updated based on:  
     - The time interval (`event_interval`) since the last event.  
     - If `speed_smooth_window_microseconds` is enabled, `event_interval` is computed as the average interval of events inside that smoothing window instead of the raw interval to the last event.
     - The `speed_factor`, which scales the speed adjustment.  
     - Clamping to ensure the speed change stays within bounds.  

   The actual speed is calculated as:  

   ```text
   actual_speed = max(initial_speed, clamp(speed_factor / event_interval, current_speed + min(current_speed * min_speed_change_ratio, max_speed_change_lowerbound), current_speed + max(current_speed * max_speed_change_ratio, min_speed_change_upperbound)))
   ```

3. **Decay Over Time**:  
   - The speed decays exponentially based on the `damping` factor:  

     ```text
     current_speed = actual_speed * exp(-damping * time_since_last_event)
     ```  

   - If the deceleration caused by `damping` is weaker than `min_deceleration`, the deceleration is clamped to `min_deceleration`. This ensures **linear deceleration** at low speeds for a more predictable stop.  
   - The computed deceleration is also clamped by `max_deceleration` to prevent excessively large instantaneous deceleration at high speeds.
   - If `current_speed` drops below zero, it resets to zero (stopping the motion).  

### Distance Model

Each accepted wheel event adds `wheel_tick_distance` to `distance_remaining_`. Unlike the Speed model, receiving events more quickly does not change the total distance contributed by each event; it only increases the outstanding budget.

To release the budget smoothly, the model calculates the speed from which the configured damping and deceleration curve would stop over the remaining distance. For `damping > 0`, let:

```text
k = damping
a_min = min_deceleration
a_max = max_deceleration
v_low = a_min / k
d_low = v_low² / (2 * a_min)
d_high = d_low + (a_max - a_min) / k²
```

The target speed for remaining distance `d` is:

```text
                    sqrt(2 * a_min * d)                              if d <= d_low
speed_for_distance = v_low + k * (d - d_low)                         if d <= d_high
                    sqrt((a_max / k)² + 2 * a_max * (d - d_high))    otherwise
```

When `damping = 0`, only the minimum-deceleration branch applies: `speed_for_distance = sqrt(2 * a_min * d)`.

For each synthetic-event tick:

```text
distance_delta = min(speed_for_distance(distance_remaining_) * tick_interval,
                     distance_remaining_)
```

`distance_delta` is subtracted from the remaining budget. A fractional rounding accumulator ensures that the emitted integer `REL_WHEEL_HI_RES` values add up to the configured distance without losing sub-unit displacement.

### Hybrid Model

Hybrid mode maintains both Speed state and the shared distance budget. Every accepted wheel event contributes `wheel_tick_distance` to `distance_remaining_`, while event timing continues to update the Speed model normally.

At the initial event and every subsequent synthetic-event tick, the model computes two non-negative displacement magnitudes:

```text
speed_delta = displacement produced by the Speed model
distance_delta = displacement required by the remaining distance budget
output_delta = max(speed_delta, distance_delta)
```

The direction sign is applied only after selecting `output_delta`. The implementation therefore does not directly call `max()` on signed wheel-event values, which would be incorrect for negative-direction scrolling.

When Speed produces the larger displacement, that output also consumes `distance_remaining_`. The distance budget can therefore reach zero while the Speed model continues scrolling. When Distance produces the larger displacement, it acts as a smooth lower bound rather than starting a second, independent scroll curve. This avoids adding both outputs together or releasing the same distance twice.

### Free Spin

While Free Spin is held:

- The Speed state does not decay.
- Distance and Hybrid modes continue emitting smooth output but do not subtract it from `distance_remaining_`.
- Hybrid mode continues to emit `max(speed_delta, distance_delta)`.

Distance and Hybrid modes use a separate fractional accumulator for Free Spin output. This allows sub-unit per-tick displacement to produce events over time without changing the rounding state of the preserved distance budget.

When Free Spin is released, normal decay resumes and the preserved distance budget continues to be consumed. Consequently, Free Spin output is additional to the configured minimum distance rather than part of that minimum.

### Braking Logic  

Three methods can stop the scrolling:

1. **Click-to-Stop**:  
   - A mouse click clears both the Speed state and the remaining distance budget.

2. **Reverse-Scroll Braking**:  
   - A wheel event in the **opposite direction** clears the active Speed state and distance budget.
   - To prevent accidental reverse-scroll jitter, opposite direction events within `max_reverse_scroll_braking_microseconds` and less than `max_reverse_scroll_braking_times - 1` after braking are ignored.  
   - All smoothing modes use `reverse_scroll_intent_window_microseconds` to distinguish continuous reverse scrolling from braking followed by a new scroll. The window starts at `last_brake_stop_time_`, the first reverse-braking event.
   - If scrolling resumes inside the intent window, Speed and Hybrid modes use the absorbed event intervals for their reverse initial-speed estimate, while Distance and Hybrid modes restore the absorbed events as distance-budget ticks. After the window expires, the current event starts a new scroll without either carry-over.

3. **Mouse Movement Braking**  
   - Mouse movement only begins accumulating after `mouse_movement_delay_microseconds` has elapsed since the last wheel event.
   - Mouse movement (X and Y axes) is tracked over a sliding time window defined by `mouse_movement_window_milliseconds`.
   - The system calculates the cumulative 2D vector distance of all movements within this active window.
   - If the distance exceeds `max_mouse_movement_distance`, both the Speed state and remaining distance budget are cleared, instantly stopping the motion.

### Key Parameters  

| Parameter | Description |  
| --------- | ----------- |  
| `smooth_mode` | Selects Speed (`0`), Distance (`1`), or Hybrid (`2`) smoothing. |
| `wheel_tick_distance` | Distance contributed by each accepted event in Distance mode and the per-event minimum-distance budget in Hybrid mode. |
| `tick_interval_microseconds` | Interval between synthetic event generations. |  
| `initial_speed` | Base speed when scrolling starts. |  
| `speed_factor` | Scales speed adjustments per wheel event. |  
| `speed_smooth_window_microseconds` | Uses a sliding time window to compute average event interval for speed estimation. |
| `damping` | Controls how quickly speed decays over time. |  
| `min_deceleration` | Minimum deceleration force (ensures linear slowdown at low speeds). |
| `max_deceleration` | Maximum deceleration force (upper bound for computed deceleration). |
| `max_speed_change_lowerbound` | The upper bound of the speed change lower bound. |  
| `min_speed_change_upperbound` | The lower bound of the speed change upper bound. |  
| `min_speed_change_ratio` | Minimum speed change ratio per wheel event. |  
| `max_speed_change_ratio` | Maximum speed change ratio per wheel event. |  
| `use_reverse_scroll_braking` | Whether reverse-scroll braking is enabled. |  
| `max_reverse_scroll_braking_microseconds` | Time window to ignore jitter after braking. |  
| `max_reverse_scroll_braking_times` | Maximum reverse-scroll braking event times. |
| `reverse_scroll_intent_window_microseconds` | Time from the first reverse brake during which absorbed events are treated as one continuous reverse scroll in all smoothing modes. |
| `use_mouse_movement_braking` | Whether mouse movement triggers braking. |  
| `max_mouse_movement_distance` | Maximum allowed 2D movement distance within the time window before scrolling stops. |
| `mouse_movement_window_milliseconds` | The sliding time window (in milliseconds) used to track recent mouse movements. |
| `mouse_movement_delay_microseconds` | The delay (in microseconds) after the last wheel event before mouse movements begin accumulating for braking. |
