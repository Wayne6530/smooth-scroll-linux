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

## 扩展与外部工具

Smooth Scroll Linux 提供了一个基于共享内存的无锁（Lock-free）IPC 协议 (`/dev/shm/smooth_scroll_shm`)。它允许外部应用程序（如托盘图标、自动化脚本或桌面环境扩展）以真正的零延迟监控和控制守护进程。

守护进程安装包现在只包含核心服务、守护进程二进制文件和默认配置。可选集成都放在 `extensions/` 下独立构建和安装，用户可以按需选择需要的扩展。

```text
extensions/
  cli/      命令行工具
  gnome/    未来的 GNOME Shell 扩展
  kde/      未来的 KDE Plasma 集成
```

### CLI 扩展

CLI 工具作为可选扩展维护在 `extensions/cli`。

编译：

```bash
cmake -S extensions/cli -B build-cli -DCMAKE_BUILD_TYPE=Release
cmake --build build-cli --parallel
```

安装到 `/usr/local`：

```bash
sudo cmake --install build-cli
```

也可以安装到 `/usr`：

```bash
sudo cmake --install build-cli --prefix /usr
```

该扩展包含：

- **`ss-status`**: 以 JSONL (JSON Lines) 格式持续监听并输出守护进程的当前状态。适合配合 `jq`、Node.js 或 Python 进行数据流解析。

  ```bash
  # 示例输出
  {"pid":12345,"connected":true,"passthrough":false,"drag_view":false,"free_spin":false,"horizontal":false,"direction":"positive","speed":150}
  ```

- **`ss-stop`**: 向守护进程发送异步刹车信号，立即终止当前正在进行的惯性滑动。
- **`ss-passthrough`**: 切换或设置强制透传（Force Passthrough）状态。在透传状态下，所有滚轮事件将跳过平滑算法直接发往系统。

  ```bash
  ss-passthrough       # 切换状态
  ss-passthrough 1     # 开启透传
  ss-passthrough off   # 关闭透传
  ```

### 开发者指南

如果你想为 Smooth Scroll Linux 开发自己的 GUI 前端或状态栏插件，可以通过读取系统的共享内存直接与守护进程通信，无需经过任何 Socket 或网络协议。

- 请参阅 [IPC Protocol](https://github.com/Wayne6530/smooth-scroll-linux/blob/main/docs/ipc_protocol.md) 了解详细的 32 字节内存布局。
- 标准 C++ 协议契约位于 `include/smooth_scroll/ipc_protocol.h`。
- 你也可以直接参考源码中 `extensions/cli/include/smooth_scroll/ipc_client.h` 的标准 C++ 实现。
