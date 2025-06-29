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

**Key Features**:

- **Universal Compatibility**: Uses Linux `uinput` to emulate a virtual mouse, ensuring broad support across distributions.  
- **Physics-Based Motion**: Mimics real-world inertia and damping, mirroring the smooth scrolling of Android/iOS.  
- **Customizable Feel**: Tweak parameters to match your preferred scrolling style.  
- **Jitter-Free Input**: Smoothens raw wheel events to eliminate sudden jumps or stutters.  
- **Smart Stopping**: Supports multiple stopping methods (click-to-stop, slow-wheel tracking, reverse-scroll braking).  
- **Lightweight**: Runs efficiently in a single thread without hogging resources.  

Perfect for users craving precision and elegance in their desktop navigation.  

## Quick Start

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

## Customization

## Roadmap
