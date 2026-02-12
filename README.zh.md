# Smooth Scroll for Linux

[English](https://github.com/Wayne6530/smooth-scroll-linux/blob/main/README.md) | 中文

## 目录

1. [介绍](#1-介绍)
2. [快速上手](#2-快速上手)
3. [个性化](#3-个性化)
4. [从源码编译](#4-从源码编译)
5. [FAQ](#5-faq)

## 1. 介绍

**Smooth Scroll for Linux** 是一款用于 Linux 系统的鼠标滚轮平滑工具，使普通鼠标拥有媲美触摸板或者高精度滚轮鼠标（如Logitech MX Master 3S）的丝滑屏幕滚动体验。

[screencast_smooth_scroll.webm](https://github.com/user-attachments/assets/d0ec740d-df2c-4257-bd15-e7a1d66b0092)

### 特性

- 基于滚轮速度的平滑滚动，彻底告别画面突跳
- 类似 Android/IOS 的滚动阻尼
- 高度可自定义的平滑参数
- 特色的 **Free Spin** 模式，一键释放滚动阻尼，专为长文档浏览设计
- 多种停止滚动的方式
- 轻量

## 2. 快速上手

### 安装

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

> **注意：** 如果没有找到适配您系统的安装包（例如 Arch Linux 或较旧的发行版），请参阅 [从源码编译](#4-从源码编译) 章节。

安装完成后，**smooth-scroll.service** 会立刻自动启动并且开机自启。

### 使用方法

1. 找个你喜欢的界面，正常使用鼠标滚轮
   - 屏幕滚动不再是一格一格的，而是平滑的追随鼠标滚轮的速度
   - 即便停下鼠标滚轮，屏幕滚动也不会立刻停止，而是会像拥有惯性一样缓慢停止
   - 如果你的现象与上述不符，请先参照 [FAQ](#5-faq) 自行排查
2. 尝试多种停止滚动的方式
   1. 使用反向滚轮（推荐）
      - 在屏幕还在滚动时，向相反方向短暂滚动鼠标滚轮，屏幕会立刻停止滚动
      - 过长时间或者过多次数的反向滚轮会导致屏幕反向滚动
      - 参考 [调整停止参数](#调整停止参数) 调节你喜欢的停止手感
   2. 点击鼠标按键（除了 **Free Spin** 按键）
   3. 连续移动鼠标
      - 在屏幕还在滚动时，连续移动鼠标一段距离，屏幕会立刻停止滚动
      - 这种方式主要用于移动鼠标指针到其他界面时，能够停止屏幕滚动
3. 尝试 **Free Spin** 模式
   - 打开一个长文档，滚动屏幕，在屏幕还在滚动时，按下并保持 **Free Spin** 键（默认为鼠标右键）
   - 屏幕将保持连续丝滑的滚动，在这期间，你可以正常使用滚轮加快滚动速度，使用任意停止滚动的方式
   - 松开 **Free Spin** 按键，屏幕滚动将缓慢停止
   - 在屏幕滚动缓慢停止期间，你可以再次按下并保持 **Free Spin** 键，通过这种方式获得你想要的滚动速度

### 管理

你可以像管理任何其他服务一样，使用 `systemctl` 进行管理，或者使用 `journalctl` 查看日志。

```bash
# 查看服务状态
systemctl status smooth-scroll.service

# 手动重启服务
sudo systemctl restart smooth-scroll.service

# 手动停止服务
sudo systemctl stop smooth-scroll.service

# 手动开启服务
sudo systemctl start smooth-scroll.service

# 查看最新日志
journalctl -xe -u smooth-scroll.service

# 实时查看日志
journalctl -xe -f -u smooth-scroll.service
```

## 3. 个性化

你可以编辑 `/etc/smooth-scroll/smooth-scroll.toml` 修改参数，然后手动重启服务，从而应用最新的参数。

### 调整滚动参数

- `damping`：滚动阻尼
  - 滚动速度越大，减速越快
  - 值越大，减速越快
  - 为 0 时，减速度只取决于 `min_deceleration`
- `min_deceleration`：最小减速度
  - 值越大，低速滚动时，减速越快
- `max_deceleration`：最大减速度
  - 值越小，高速滚动时，减速越慢，滚动距离越远
- `initial_speed`：初始速度
  - 从停止开始滚动的初始速度
  - 值越大，单次滚轮引起的滚动距离越大
- `speed_factor`：速度系数
  - 值越大，速度越大

举例：

1. 增大一次滚轮滚动的距离
   - 增大 `initial_speed`
   - 减小 `min_deceleration`
2. 保持一次滚轮滚动距离不变，减小滚动的时间
   - 同时增大 `initial_speed` 和 `min_deceleration`
3. 更加顺滑
   - 减小 `damping`
4. 增大最高滚动速度
   - 增大 `speed_factor`
   - 减小 `damping`
   - 减小 `max_deceleration`

### 调整停止参数

- `use_braking`：是否使用反向滚轮停止功能
- `braking_dejitter_microseconds`：反向滚轮停止的最长时间
- `max_braking_times`：反向滚轮停止的最多次数
- `use_mouse_movement_braking`：是否使用连续移动鼠标停止功能

### 使用调试模式

开启调试模式可以直观地从数据上观察参数造成的影响。

1. 手动停止服务
2. 使用 `sudo smooth-scroll -d -c /etc/smooth-scroll/smooth-scroll.toml` 指令开启调试输出

### 高级自定义

以上没有提到的参数，对于绝大多数用户都没有必要。如果你想深入了解内部的工作原理，并解锁非常规的使用方法，请阅读 [Technical Insight](https://github.com/Wayne6530/smooth-scroll-linux/blob/main/docs/technical_insight.md)。

## 4. 从源码编译

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

   # 安装 (将安装至 /usr/bin /usr/lib/systemd/system 和 /etc/smooth-scroll)
   sudo make install
   ```

3. 启用服务

   ```bash
   sudo systemctl enable --now smooth-scroll
   ```

## 5. FAQ

### 为什么开始滚动时有死区

这是 `libinput` 中的一个已知问题 [Disable hi-res wheel event initial accumulation for uinput (#1129)](https://gitlab.freedesktop.org/libinput/libinput/-/issues/1129)。该问题已经修复但未必在你的系统中发布，你可以参照以下步骤手动编译并安装。

1. 安装依赖

   ```bash
   sudo apt install meson ninja-build libmtdev-dev libevdev-dev libudev-dev libwacom-dev
   ```

2. 下载并编译

   ```bash
   git clone https://gitlab.freedesktop.org/libinput/libinput.git
   cd libinput
   meson setup builddir --prefix=/usr -Ddocumentation=false -Dtests=false -Ddebug-gui=false
   ninja -C builddir
   ```

3. 安装

   ```bash
   sudo ninja -C builddir install
   ```

4. 重启系统
