# Smooth Scroll Linux

[](https://github.com/user-attachments/assets/9398989b-ec77-4d03-ab3d-4967d37600db)

<p align="center"><strong>Smooth Scroll Linux</strong> is a system-level smooth scrolling daemon that brings physics-based inertial scrolling to any regular mouse on Linux.</p>

<p align="center">
  <a href="https://wayne6530.github.io/smooth-scroll-linux/quickstart.html"><strong>Quick Start</strong></a>
  ·
  <a href="https://wayne6530.github.io/smooth-scroll-linux/config.html">Web Configurator</a>
  ·
  <a href="https://wayne6530.github.io/smooth-scroll-linux/faq.html">FAQ</a>
  ·
  <a href="README.zh.md">中文</a>
</p>

## Installation

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

   # Install to /usr/bin, /usr/lib/systemd/system, and /etc/smooth-scroll
   sudo make install
   ```

3. Enable service:

   ```bash
   sudo systemctl enable --now smooth-scroll
   ```

## Extensions

Optional extensions add command-line tools and desktop-environment integration
on top of the core daemon.

Current extensions:

- **CLI tools** (`extensions/cli`): terminal commands for reading daemon status,
  stopping active inertial scrolling, and toggling force passthrough.
- **GNOME Shell extension** (`extensions/gnome`): pointer-side indicators,
  pointer-leave braking, and per-window force passthrough rules for GNOME.
- **KDE Plasma KWin effect** (`extensions/kde`): pointer-side indicators,
  pointer-leave braking, per-application force passthrough rules, and a
  window-based rule picker for KDE Plasma.

![GNOME pointer-side indicators](extensions/gnome/assets/pointer-side-indicators.svg)

Each extension has its own README with detailed build, install, and usage
instructions.

### For Developers

If you want to build your own extension, GUI frontend, or status bar widget for
Smooth Scroll Linux, communicate with the daemon through the shared-memory IPC
file at `/dev/shm/smooth_scroll_shm`.

- Read the [IPC Protocol](https://github.com/Wayne6530/smooth-scroll-linux/blob/main/docs/ipc_protocol.md) for the 32-byte memory layout.
- The canonical C++ protocol contract lives at `include/smooth_scroll/ipc_protocol.h`.
- The CLI extension includes a small C++ IPC client at `extensions/cli/include/smooth_scroll/ipc_client.h`.
