# Smooth Scroll for Linux

English | [中文](https://github.com/Wayne6530/smooth-scroll-linux/blob/main/README.zh.md)

## Table of Contents

1. [Introduction](#introduction)
2. [Quick Start](#quick-start)
3. [Customization](#customization)
4. [Build from Source](#build-from-source)
5. [FAQ](#faq)

## Introduction

**Smooth Scroll for Linux** is a tool for Linux systems that brings smooth, touchpad-like or high-end mouse (such as Logitech MX Master 3S) scrolling to any regular mouse.

[screencast_smooth_scroll.webm](https://github.com/user-attachments/assets/d0ec740d-df2c-4257-bd15-e7a1d66b0092)

### Features

- Smooth scrolling based on wheel speed, eliminating jumpy movement
- Android/iOS-like scroll damping
- Highly customizable smoothness parameters
- Unique **Free Spin** mode for effortless long-document navigation
- Multiple ways to stop scrolling
- Lightweight and efficient

## Quick Start

### Installation

Download the latest package from the [Release](https://github.com/Wayne6530/smooth-scroll-linux/releases) page. In your download directory, run:

```bash
sudo apt install ./smooth-scroll_0.2.0_amd64.deb
```

After installation, **smooth-scroll.service** will start automatically and enable itself at boot.

### Usage

1. Use your mouse wheel as usual in any application.
   - Scrolling will now be smooth and follow the speed of your wheel.
   - When you stop the wheel, scrolling will decelerate gradually, simulating inertia.
   - If this doesn't work as described, check the [FAQ](#faq).
2. Try different ways to stop scrolling:
   1. **Reverse wheel** (recommended): Briefly scroll in the opposite direction while scrolling is active to stop immediately. Too many or too long reverse scrolls may cause reverse scrolling. Adjust the stop parameters in the config for your preferred feel.
   2. **Click any mouse button** (except the **Free Spin** button).
   3. **Move the mouse pointer**: Moving the mouse a certain distance while scrolling will stop the scroll, useful when switching to another window.
3. Try **Free Spin** mode:
   - Open a long document, start scrolling, then hold the **Free Spin** button (default: right mouse button) while scrolling is active.
   - Scrolling will continue smoothly; you can increase speed with the wheel or use any stop method.
   - Release the **Free Spin** button to gradually stop.
   - You can re-engage **Free Spin** during deceleration to adjust speed as needed.

### Service Management

Manage the service with `systemctl` or view logs with `journalctl`:

```bash
# Check service status
systemctl status smooth-scroll.service

# Restart service
sudo systemctl restart smooth-scroll.service

# Stop service
sudo systemctl stop smooth-scroll.service

# Start service
sudo systemctl start smooth-scroll.service

# View latest logs
journalctl -xe -u smooth-scroll.service

# View logs in real time
journalctl -xe -f -u smooth-scroll.service
```

## Customization

Edit `/etc/smooth-scroll/smooth-scroll.toml` to change parameters, then restart the service to apply changes.

### Scroll Parameters

- `damping`: Scroll damping. Higher values decelerate faster. If 0, only `min_deceleration` applies.
- `min_deceleration`: Minimum deceleration. Higher values decelerate faster at low speeds.
- `max_deceleration`: Maximum deceleration. Smaller values reduce deceleration strength at high speeds, making high-speed scrolls last longer and travel farther.
- `initial_speed`: Initial speed when starting to scroll. Higher values increase distance per wheel tick.
- `speed_factor`: Speed multiplier. Higher values increase speed, limited by `max_speed_increase_per_wheel_event`.
- `max_speed_increase_per_wheel_event`: Max speed increase per wheel event.

Examples:

1. **Increase scroll distance per tick**: Increase `initial_speed`, decrease `min_deceleration`.
2. **Shorten scroll time without changing distance**: Increase both `initial_speed` and `min_deceleration`.
3. **Make scrolling smoother**: Decrease `damping`.
4. **Increase max scroll speed**: Increase `max_speed_increase_per_wheel_event` and `speed_factor`, decrease `damping` and `max_deceleration`.

### Stop Parameters

- `use_braking`: Enable reverse wheel stop.
- `braking_dejitter_microseconds`: Max time for reverse wheel stop.
- `max_braking_times`: Max reverse wheel stop count.
- `use_mouse_movement_braking`: Enable stop by mouse movement.

### Debug Mode

Enable debug mode to observe parameter effects:

1. Stop the service.
2. Run:  

   ```bash
   sudo smooth-scroll -d -c /etc/smooth-scroll/smooth-scroll.toml
   ```

### Advanced Customization

For advanced users, see [Technical Insight](https://github.com/Wayne6530/smooth-scroll-linux/blob/main/docs/technical_insight.md) for more parameters and internal details.

## Build from Source

1. Install dependencies:

   ```bash
   sudo apt install build-essential cmake libspdlog-dev libevdev-dev
   ```

2. Clone and build:

   ```bash
   git clone https://github.com/Wayne6530/smooth-scroll-linux.git
   cd smooth-scroll-linux
   cmake -B build -DCMAKE_BUILD_TYPE=Release
   cd build
   make package
   ```

```text
Note:
Do not use make install, as this will install to /usr/local/bin/smooth-scroll, which does not match the service's expected location (/usr/bin/smooth-scroll).
```

## FAQ

### Why is there a dead zone at the start of scrolling?

This is a known issue in `libinput` ([Disable hi-res wheel event initial accumulation for uinput (#1129)](https://gitlab.freedesktop.org/libinput/libinput/-/issues/1129)). It has been fixed upstream but may not be released for your system yet. You can manually compile and install the latest version:

1. Install dependencies:

   ```bash
   sudo apt install meson ninja-build libmtdev-dev libevdev-dev libudev-dev libwacom-dev
   ```

2. Clone and build:

   ```bash
   git clone https://gitlab.freedesktop.org/libinput/libinput.git
   cd libinput
   meson setup builddir --prefix=/usr -Ddocumentation=false -Dtests=false -Ddebug-gui=false
   ninja -C builddir
   ```

3. Install:

   ```bash
   sudo ninja -C builddir install
   ```

4. Restart your desktop session or input-related services to apply the changes.
