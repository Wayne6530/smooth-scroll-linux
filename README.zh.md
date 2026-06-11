# Smooth Scroll Linux

[](https://github.com/user-attachments/assets/9398989b-ec77-4d03-ab3d-4967d37600db)

<p align="center"><strong>Smooth Scroll Linux</strong> 是一个 Linux 系统级平滑滚动守护进程，让普通鼠标滚轮获得带物理惯性的顺滑滚动体验。</p>

<p align="center">
  <a href="https://wayne6530.github.io/smooth-scroll-linux/quickstart.html"><strong>快速开始</strong></a>
  ·
  <a href="https://wayne6530.github.io/smooth-scroll-linux/config.html">网页配置工具</a>
  ·
  <a href="https://wayne6530.github.io/smooth-scroll-linux/faq.html">FAQ</a>
</p>

## 安装

1. 前往 [Releases](https://github.com/Wayne6530/smooth-scroll-linux/releases) 页面。
2. 下载适合您发行版的安装包：

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

> **注意：** 如果没有找到适配您系统的安装包（例如 Arch Linux 或较旧的发行版），请参阅 [从源码编译](#从源码编译) 章节。

安装完成后，**smooth-scroll.service** 会立刻自动启动并且开机自启。

## 从源码编译

### DEB

1. 安装依赖

   ```bash
   sudo apt install build-essential cmake libspdlog-dev libevdev-dev
   ```

2. 下载源码并编译

   ```bash
   git clone https://github.com/Wayne6530/smooth-scroll-linux.git
   cd smooth-scroll-linux
   cmake -B build -DCMAKE_BUILD_TYPE=Release -DCPACK_GENERATOR="DEB"
   cd build
   make package
   ```

### RPM

1. 安装依赖

   ```bash
   sudo dnf install gcc-c++ cmake spdlog-devel libevdev-devel rpm-build
   ```

2. 下载源码并编译

   ```bash
   git clone https://github.com/Wayne6530/smooth-scroll-linux.git
   cd smooth-scroll-linux
   cmake -B build -DCMAKE_BUILD_TYPE=Release -DCPACK_GENERATOR="RPM"
   cd build
   make package
   ```

### Arch Linux / Manjaro

1. 安装依赖

   ```bash
   sudo pacman -S base-devel cmake git spdlog libevdev
   ```

2. 下载源码并编译安装

   ```bash
   git clone https://github.com/Wayne6530/smooth-scroll-linux.git
   cd smooth-scroll-linux
   cmake -B build -DCMAKE_BUILD_TYPE=Release
   cd build
   make

   # 安装至 /usr/bin、/usr/lib/systemd/system 和 /etc/smooth-scroll
   sudo make install
   ```

3. 启用服务

   ```bash
   sudo systemctl enable --now smooth-scroll
   ```

## 扩展

可选扩展为核心守护进程提供命令行工具和桌面环境集成。

当前扩展：

- **CLI 工具**（`extensions/cli`）：用于读取守护进程状态、停止当前惯性滚动、切换强制透传的终端命令。
- **GNOME Shell 扩展**（`extensions/gnome`）：为 GNOME 提供指针旁状态指示、离开窗口刹车，以及按窗口规则设置强制透传。
- **KDE Plasma KWin effect**（`extensions/kde`）：为 KDE Plasma 提供指针旁状态指示、离开窗口刹车、按应用规则设置强制透传，以及基于窗口拾取的规则配置工具。

![GNOME 指针旁状态指示](extensions/gnome/assets/pointer-side-indicators.svg)

每个扩展都有自己的 README，详细说明构建、安装和使用方式。

### 开发者指南

如果你想为 Smooth Scroll Linux 开发自己的扩展、GUI 前端或状态栏插件，
可以通过 `/dev/shm/smooth_scroll_shm` 这个共享内存 IPC 文件与守护进程通信。

- 请参阅 [IPC Protocol](https://github.com/Wayne6530/smooth-scroll-linux/blob/main/docs/ipc_protocol.md) 了解 32 字节内存布局。
- 标准 C++ 协议契约位于 `include/smooth_scroll/ipc_protocol.h`。
- CLI 扩展提供了一个小型 C++ IPC 客户端：`extensions/cli/include/smooth_scroll/ipc_client.h`。
