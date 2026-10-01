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

The core daemon requires CMake 3.14+, a C++17 compiler, and a Linux system using systemd.
CMake downloads tomlplusplus during configuration, so an internet connection is required.

### Install Dependencies

#### Ubuntu / Debian / Linux Mint

```bash
sudo apt update
sudo apt install build-essential cmake git libfmt-dev libspdlog-dev libevdev-dev libudev-dev
```

#### Fedora

```bash
sudo dnf install gcc-c++ make cmake git fmt-devel spdlog-devel libevdev-devel systemd-devel
```

#### Arch Linux / Manjaro / CachyOS

```bash
sudo pacman -S --needed base-devel cmake git fmt spdlog libevdev systemd-libs
```

After installing dependencies, clone the repository and choose either direct installation or package building below:

```bash
git clone https://github.com/Wayne6530/smooth-scroll-linux.git
cd smooth-scroll-linux
```

### Build and Install

These steps apply to any systemd-based Linux distribution with the required dependencies installed.
Run them from the repository root:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel

# Install to /usr/bin, /usr/lib/systemd/system, and /etc/smooth-scroll
sudo cmake --build build --target install
sudo systemctl daemon-reload
sudo systemctl enable --now smooth-scroll
```

### Build Packages

To generate an installable package instead of installing directly, use the matching section below.
Run each build from the repository root. The current packaging configuration targets x86_64.

#### DEB

On Ubuntu / Debian / Linux Mint:

```bash
cmake -S . -B build-deb -DCMAKE_BUILD_TYPE=Release -DCPACK_GENERATOR="DEB"
cmake --build build-deb --parallel
cd build-deb
cpack -G DEB
sudo apt install ./smooth-scroll_*.deb
```

#### RPM

On Fedora, install the additional packaging tools first:

```bash
sudo dnf install rpm-build
cmake -S . -B build-rpm -DCMAKE_BUILD_TYPE=Release -DCPACK_GENERATOR="RPM"
cmake --build build-rpm --parallel
cd build-rpm
cpack -G RPM
sudo dnf install ./smooth-scroll-*.rpm
```

The DEB and RPM packages start the service automatically and enable it at boot.

## Extensions

Optional extensions add command-line tools and desktop-environment integration
on top of the core daemon.

Current extensions:

- **CLI tools** (`extensions/cli`): terminal commands for reading daemon status,
  stopping active inertial scrolling, and requesting compatibility passthrough.
- **GNOME Shell extension** (`extensions/gnome`): pointer-side indicators,
  pointer-leave braking, and per-window compatibility-passthrough rules for GNOME.
- **KDE Plasma KWin effect** (`extensions/kde`): pointer-side indicators,
  pointer-leave braking, per-application compatibility-passthrough rules, and a
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

## Contributing

The core daemon is desktop-environment agnostic. Its architecture, behavior,
and overall design direction are maintainer-led to keep the core focused and
consistent.

Desktop integrations under `extensions/` are intended to be maintained
collaboratively with the community. Contributions covering implementation,
compatibility, packaging, testing, and ongoing maintenance across different
desktop environments and versions are very welcome.
