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

核心守护进程需要 CMake 3.14+、支持 C++17 的编译器，以及使用 systemd 的 Linux 系统。
CMake 会在配置阶段下载 tomlplusplus，因此需要联网。

### 安装依赖

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

安装依赖后，下载源码，再选择下方的直接安装或生成安装包流程：

```bash
git clone https://github.com/Wayne6530/smooth-scroll-linux.git
cd smooth-scroll-linux
```

### 编译并安装

以下步骤适用于已安装所需依赖、使用 systemd 的 Linux 发行版。
请在仓库根目录执行：

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel

# 安装至 /usr/bin、/usr/lib/systemd/system 和 /etc/smooth-scroll
sudo cmake --build build --target install
sudo systemctl daemon-reload
sudo systemctl enable --now smooth-scroll
```

### 生成安装包

如果希望生成安装包而非直接安装，请选择下方对应流程。
每种打包流程均从仓库根目录开始执行。当前打包配置面向 x86_64 架构。

#### DEB

在 Ubuntu / Debian / Linux Mint 上执行：

```bash
cmake -S . -B build-deb -DCMAKE_BUILD_TYPE=Release -DCPACK_GENERATOR="DEB"
cmake --build build-deb --parallel
cd build-deb
cpack -G DEB
sudo apt install ./smooth-scroll_*.deb
```

#### RPM

在 Fedora 上，先安装额外的打包工具：

```bash
sudo dnf install rpm-build
cmake -S . -B build-rpm -DCMAKE_BUILD_TYPE=Release -DCPACK_GENERATOR="RPM"
cmake --build build-rpm --parallel
cd build-rpm
cpack -G RPM
sudo dnf install ./smooth-scroll-*.rpm
```

DEB 和 RPM 安装包会自动启动服务并启用开机自启。

## 扩展

可选扩展为核心守护进程提供命令行工具和桌面环境集成。

当前扩展：

- **CLI 工具**（`extensions/cli`）：用于读取守护进程状态、停止当前惯性滚动、请求兼容性透传的终端命令。
- **GNOME Shell 扩展**（`extensions/gnome`）：为 GNOME 提供指针旁状态指示、离开窗口刹车，以及按窗口规则设置兼容性透传。
- **KDE Plasma KWin effect**（`extensions/kde`）：为 KDE Plasma 提供指针旁状态指示、离开窗口刹车、按应用规则设置兼容性透传，以及基于窗口拾取的规则配置工具。

![GNOME 指针旁状态指示](extensions/gnome/assets/pointer-side-indicators.svg)

每个扩展都有自己的 README，详细说明构建、安装和使用方式。

### 开发者指南

如果你想为 Smooth Scroll Linux 开发自己的扩展、GUI 前端或状态栏插件，
可以通过 `/dev/shm/smooth_scroll_shm` 这个共享内存 IPC 文件与守护进程通信。

- 请参阅 [IPC Protocol](https://github.com/Wayne6530/smooth-scroll-linux/blob/main/docs/ipc_protocol.md) 了解 32 字节内存布局。
- 标准 C++ 协议契约位于 `include/smooth_scroll/ipc_protocol.h`。
- CLI 扩展提供了一个小型 C++ IPC 客户端：`extensions/cli/include/smooth_scroll/ipc_client.h`。

## 参与贡献

核心守护进程不依赖特定桌面环境。为保持核心功能聚焦且设计一致，其架构、
行为和总体设计方向由维护者主导。

`extensions/` 下的桌面集成由社区共同参与维护。非常欢迎针对不同桌面环境和
版本贡献功能实现、兼容性适配、打包、测试及长期维护工作。
