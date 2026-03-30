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

### Speed Calculation  

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

### Braking Logic  

Two methods can stop the scrolling:  

1. **Click-to-Stop**:  
   - A mouse click forces `current_speed = 0`.  

2. **Reverse-Scroll Braking**:  
   - A wheel event in the **opposite direction** sets the speed to 0.  
   - To prevent accidental reverse-scroll jitter, opposite direction events within `max_reverse_scroll_braking_microseconds` and less than `max_reverse_scroll_braking_times - 1` after braking are ignored.  

3. **Mouse Movement Braking**  
   - Mouse movement (X and Y axes) is tracked over a sliding time window defined by `mouse_movement_window_milliseconds`.
   - The system calculates the cumulative 2D vector distance of all movements within this active window.
   - If the distance exceeds `max_mouse_movement_distance`, the scrolling speed immediately resets to zero, instantly stopping the motion.

### Key Parameters  

| Parameter | Description |  
| --------- | ----------- |  
| `tick_interval_microseconds` | Interval between synthetic event generations. |  
| `initial_speed` | Base speed when scrolling starts. |  
| `speed_factor` | Scales speed adjustments per wheel event. |  
| `speed_smooth_window_microseconds` | Uses a sliding time window (microseconds) to compute average event interval for speed estimation. |
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
| `use_mouse_movement_braking` | Whether mouse movement triggers braking. |  
| `max_mouse_movement_distance` | Maximum allowed 2D movement distance within the time window before scrolling stops. |
| `mouse_movement_window_milliseconds` | The sliding time window (in milliseconds) used to track recent mouse movements. |
