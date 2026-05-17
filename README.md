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

## Integration & External Tools

Smooth Scroll Linux provides a lock-free shared memory IPC protocol (`/dev/shm/smooth_scroll_shm`). This allows external applications, such as system tray icons, automation scripts, or desktop environment extensions, to monitor and control the daemon with true zero latency.

### Included CLI Utilities

When you install or build the project, three CLI utilities are automatically included for terminal use or script integration:

- **`ss-status`**: Continuously listens to and outputs the daemon's state in JSONL (JSON Lines) format. This is suitable for streaming and parsing with `jq`, Node.js, or Python.

  ```bash
  # Example output
  {"pid":12345,"connected":true,"passthrough":false,"drag_view":false,"free_spin":false,"horizontal":false,"direction":"positive","speed":150}
  ```

- **`ss-stop`**: Sends an asynchronous brake signal to the daemon, immediately halting any ongoing inertial scrolling.
- **`ss-passthrough`**: Toggles or sets the "Force Passthrough" state. When passthrough is active, all wheel events bypass the smoothing algorithm and are sent directly to the system.

  ```bash
  ss-passthrough       # Toggle state
  ss-passthrough 1     # Enable passthrough
  ss-passthrough off   # Disable passthrough
  ```

### For Developers

If you want to build your own GUI frontend or status bar widget for Smooth Scroll Linux, you can communicate directly with the daemon by reading the system's shared memory, avoiding socket or network overhead.

- Read the [IPC Protocol](https://github.com/Wayne6530/smooth-scroll-linux/blob/main/docs/ipc_protocol.md) for details on the 32-byte memory layout.
- You can also reference the standard C++ implementation in the source code at `tools/ipc_client.h`.
