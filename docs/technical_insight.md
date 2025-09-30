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
     - Clamping to ensure the speed change stays within bounds (`max_speed_increase_per_wheel_event` and `max_speed_decrease_per_wheel_event`).  

   The actual speed is calculated as:  

   ```text
   actual_speed = max(initial_speed, clamp(speed_factor / event_interval, current_speed - max_speed_decrease_per_wheel_event, current_speed + max_speed_increase_per_wheel_event))
   ```

3. **Decay Over Time**:  
   - The speed decays exponentially based on the `damping` factor:  

     ```text
     current_speed = actual_speed * exp(-damping * time_since_last_event)
     ```  

   - If the deceleration caused by `damping` is weaker than `min_deceleration`, the deceleration is clamped to `min_deceleration`. This ensures **linear deceleration** at low speeds for a more predictable stop.  
   - The computed deceleration is also clamped by `max_deceleration` to prevent excessively large instantaneous deceleration at high speeds.
   - If `current_speed` drops below `min_speed`, it resets to zero (stopping the motion).  

### Braking Logic  

Two methods can stop the scrolling:  

1. **Click-to-Stop**:  
   - A mouse click forces `current_speed = 0`.  

2. **Reverse-Scroll Braking**:  
   - A wheel event in the **opposite direction** reduces the speed by `speed_decrease_per_braking`.  
   - If `current_speed` falls below `braking_cut_off_speed`, it resets to zero.  
   - To prevent accidental reverse-scroll jitter, opposite direction events within `braking_dejitter_microseconds` and less than `max_braking_times - 1` after braking are ignored.  

3. **Mouse Movement Braking**  
   - When the cumulative distance of continuous mouse movement exceeds `mouse_movement_dejitter_distance`, each subsequent movement unit reduces the speed by `speed_decrease_per_mouse_movement`.  
   - If `current_speed` falls below `mouse_movement_braking_cut_off_speed`, the speed resets to zero.  
   - If the time interval between mouse movements exceeds `max_mouse_movement_event_interval_microseconds`, it's treated as a new movement sequence, and `mouse_movement_dejitter_distance` resets.  

### Key Parameters  

| Parameter | Description |  
|-----------|-------------|  
| `tick_interval_microseconds` | Interval between synthetic event generations. |  
| `initial_speed` | Base speed when scrolling starts. |  
| `speed_factor` | Scales speed adjustments per wheel event. |  
| `speed_smooth_window_microseconds` | Uses a sliding time window (microseconds) to compute average event interval for speed estimation. |
| `damping` | Controls how quickly speed decays over time. |  
| `min_speed` | Minimum speed before motion stops. |  
| `min_deceleration` | Minimum deceleration force (ensures linear slowdown at low speeds). |
| `max_deceleration` | Maximum deceleration force (upper bound for computed deceleration). |
| `max_speed_increase_per_wheel_event` | Limits how much speed can increase per event. |  
| `max_speed_decrease_per_wheel_event` | Limits how much speed can decrease per event. |  
| `use_braking` | Whether reverse-scroll braking is enabled. |  
| `braking_dejitter_microseconds` | Time window to ignore jitter after braking. |  
| `max_braking_times` | Maximum reverse-scroll braking event times. |
| `braking_cut_off_speed` | Speed threshold to stop scrolling during braking. |  
| `speed_decrease_per_braking` | Speed reduction per opposite-direction wheel event. |  
| `use_mouse_movement_braking` | Whether mouse movement triggers braking. |  
| `mouse_movement_dejitter_distance` | Minimum cumulative movement distance before braking activates. |  
| `max_mouse_movement_event_interval_microseconds` | Maximum time between movements to be considered continuous. |  
| `mouse_movement_braking_cut_off_speed` | Speed threshold to stop scrolling during mouse movement braking. |  
| `speed_decrease_per_mouse_movement` | Speed reduction per movement unit after dejitter distance. |  
