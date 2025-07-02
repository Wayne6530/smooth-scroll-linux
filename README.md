# Smooth Scroll for Linux

## Table of Contents

1. [Introduction](#introduction)
2. [Quick Start](#quick-start)
3. [FAQ](#faq)
4. [Technical Insight](#technical-insight)
5. [Customization](#customization)
6. [Roadmap](#roadmap)

## Introduction  

**Smooth Scroll for Linux** is a lightweight tool that transforms your mouse wheel scrolling into a fluid, natural experience.  

[screencast_smooth_scroll.webm](https://github.com/user-attachments/assets/d0ec740d-df2c-4257-bd15-e7a1d66b0092)

**Key Features**:

- **Universal Compatibility**: Uses Linux `uinput` to emulate a virtual mouse, ensuring broad support across distributions.  
- **Physics-Based Motion**: Mimics real-world inertia and damping, mirroring the smooth scrolling of Android/iOS.  
- **Customizable Feel**: Tweak parameters to match your preferred scrolling style.  
- **Jitter-Free Input**: Smoothens raw wheel events to eliminate sudden jumps or stutters.  
- **Smart Stopping**: Supports multiple stopping methods (click-to-stop, slow-wheel tracking, reverse-scroll braking).  
- **Lightweight**: Runs efficiently in a single thread without hogging resources.  

Perfect for users craving precision and elegance in their desktop navigation.  

## Quick Start

### Prerequisites

Ensure your system has the following dependencies installed:

- **Compiler**: GCC/Clang with C++17 support
- **Build Tools**: CMake (≥ 3.14)

### Installation Steps

1. **Install Dependencies** (Ubuntu/Debian):

   ```bash
   sudo apt install build-essential cmake libspdlog-dev libevdev-dev
   ```

2. **Clone & Build**:

   ```bash
   git clone https://github.com/Wayne6530/smooth-scroll-linux.git
   cd smooth-scroll-linux
   cmake -B build -DCMAKE_BUILD_TYPE=Release
   cmake --build build
   ```

### Finding Your Mouse Device

1. **Install `libinput-tools`**:

   ```bash
   sudo apt install libinput-tools
   ```

2. **Identify your mouse device**:
   - Run the debug command:

     ```bash
     sudo libinput debug-events
     ```

   - Move your mouse or scroll the wheel. The output will display your device ID (e.g., `event9`):

     ```text
     event9   POINTER_MOTION          322  +1.363s   0.00/  1.10 ( +0.00/ +1.00)
     ```

     The prefix (e.g., `event9`) is your device identifier.

### Configuration

1. **Copy the Config File**:  
   Run this command to copy the default configuration to your build directory:

   ```bash
   cp smooth-scroll.toml build/
   ```

2. **Modify the Device Name**:  
   Open `smooth-scroll.toml` in a text editor and update the `device` field with your mouse's event ID (e.g., `event9`).  

   Example:  

   ```toml
   device = "/dev/input/event9"  # Replace with your device ID
   ```

### Running the Tool

- **Basic Usage** (requires root permissions):

  ```bash
  cd build
  sudo ./smooth-scroll
  ```

  By default, it looks for `./smooth-scroll.toml`.

- **Custom Config Path**:

  ```bash
  sudo ./smooth-scroll -c /path/to/config.toml
  ```

## FAQ

### Why does scrolling have dead zones or sudden acceleration?

This issue typically occurs when your system uses `libinput`. In `libinput`, this is a known problem ([Disable hi-res wheel event initial accumulation for uinput (#1129)](https://gitlab.freedesktop.org/libinput/libinput/-/issues/1129)).  

The original intent was to prevent accidental scrolling when clicking the middle button on high-resolution wheel mice. However, this behavior is unnecessary for virtual high-resolution wheels.  

The issue has been fixed and merged into the `libinput` main branch, but it hasn't been officially released yet. To achieve the smoothest scrolling experience, you'll need to compile and install the latest `libinput` from source.  

Follow these steps:  

1. **Install Dependencies**:

   ```bash
   sudo apt install meson ninja-build libmtdev-dev libevdev-dev libudev-dev libwacom-dev
   ```

2. **Clone the Repository**:

   ```bash
   git clone https://gitlab.freedesktop.org/libinput/libinput.git
   cd libinput
   ```

3. **Configure and Build**:

   ```bash
   meson setup builddir --prefix=/usr -Ddocumentation=false -Dtests=false -Ddebug-gui=false
   ninja -C builddir
   ```

4. **Install**:

   ```bash
   sudo ninja -C builddir install
   ```

5. **Restart Services**:  
   After installation, restart your desktop session or input-related services to apply the changes.  

## Technical Insight

### Event Processing Pipeline  

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

### Smoothing Algorithm  

The physics-based smoothing algorithm transforms discrete wheel events into fluid motion using the following principles:

#### Speed Calculation  

The speed is measured in `REL_WHEEL_HI_RES` values per second.  

1. **Initial Trigger**:  
   - When scrolling starts (after a stop), the first wheel event sets the initial speed (`initial_speed`).  

2. **Subsequent Events**:  
   - For each new wheel event in the **same direction**, the speed is updated based on:  
     - The time interval (`event_interval`) since the last event.  
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
   - If `current_speed` drops below `min_speed`, it resets to zero (stopping the motion).  

#### Braking Logic  

Two methods can stop the scrolling:  

1. **Click-to-Stop**:  
   - A mouse click forces `current_speed = 0`.  

2. **Reverse-Scroll Braking**:  
   - A wheel event in the **opposite direction** reduces the speed by `speed_decrease_per_braking`.  
   - If `current_speed` falls below `braking_cut_off_speed`, it resets to zero.  
   - To prevent accidental reverse-scroll jitter, opposite direction events within `braking_dejitter_microseconds` after braking are ignored.  

#### Key Parameters  

| Parameter | Description |  
|-----------|-------------|  
| `initial_speed` | Base speed when scrolling starts. |  
| `speed_factor` | Scales speed adjustments per wheel event. |  
| `damping` | Controls how quickly speed decays over time. |  
| `min_speed` | Minimum speed before motion stops. |  
| `min_deceleration` | Minimum deceleration force (ensures linear slowdown at low speeds). |
| `max_speed_increase_per_wheel_event` | Limits how much speed can increase per event. |  
| `max_speed_decrease_per_wheel_event` | Limits how much speed can decrease per event. |  
| `speed_decrease_per_braking` | Speed reduction per opposite-direction event. |  
| `braking_cut_off_speed` | Threshold to stop scrolling during braking. |  
| `braking_dejitter_microseconds` | Time window to ignore jitter after braking. |  

## Customization

## Roadmap
