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

1. Go to the [Releases](https://github.com/Wayne6530/smooth-scroll-linux/releases) page.
2. Download the package matching your distribution:

**Ubuntu / Linux Mint:**

```bash
cd ~/Downloads
sudo apt install ./smooth-scroll_*.deb
```

**Fedora:**

```bash
cd ~/Downloads
sudo dnf install ./smooth-scroll-*.rpm
```

> **Note:** If a pre-built package is not available for your distribution (e.g., Arch Linux, older distros), please refer to the [Build from Source](#build-from-source) section.

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
- `speed_factor`: Speed multiplier. Higher values increase speed.

Examples:

1. **Increase scroll distance per tick**: Increase `initial_speed`, decrease `min_deceleration`.
2. **Shorten scroll time without changing distance**: Increase both `initial_speed` and `min_deceleration`.
3. **Make scrolling smoother**: Decrease `damping`.
4. **Increase max scroll speed**: Increase `speed_factor`, decrease `damping` and `max_deceleration`.

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

### DEB

1. Install dependencies:

   ```bash
   sudo apt install build-essential cmake libspdlog-dev libevdev-dev
   ```

2. Clone and build:

   ```bash
   git clone https://github.com/Wayne6530/smooth-scroll-linux.git
   cd smooth-scroll-linux
   cmake -B build -DCMAKE_BUILD_TYPE=Release -DCPACK_GENERATOR="DEB"
   cd build
   make package
   ```

### RPM

1. Install dependencies:

   ```bash
   sudo dnf install gcc-c++ cmake spdlog-devel libevdev-devel rpm-build 
   ```

2. Clone and build:

   ```bash
   git clone https://github.com/Wayne6530/smooth-scroll-linux.git
   cd smooth-scroll-linux
   cmake -B build -DCMAKE_BUILD_TYPE=Release -DCPACK_GENERATOR="RPM"
   cd build
   make package
   ```

### Arch Linux / Manjaro

1. Install dependencies:

   ```bash
   sudo pacman -S base-devel cmake git spdlog libevdev
   ```

2. Clone and build:

   ```bash
   git clone https://github.com/Wayne6530/smooth-scroll-linux.git
   cd smooth-scroll-linux
   cmake -B build -DCMAKE_BUILD_TYPE=Release
   cd build
   make

   # Install (Installs to /usr/bin /usr/lib/systemd/system and /etc/smooth-scroll)
   sudo make install
   ```

3. Enable Service:

   ```bash
   sudo systemctl enable --now smooth-scroll
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

4. Restart your desktop session.
