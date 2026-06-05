# Smooth Scroll KWin Effect

This KWin effect adds KDE Plasma integration for Smooth Scroll Linux. Use it
when the Smooth Scroll daemon is already installed and running, and you want
session-aware behavior in KDE Plasma.

It provides:

- forced passthrough when the pointer is not over a regular application window
- per-application force-passthrough rules
- inertial scrolling stop when the pointer leaves the window where scrolling
  started
- pointer-side indicators for inertial scrolling, Drag View, and forced
  passthrough

The effect talks to the daemon through Smooth Scroll Linux's shared-memory IPC
file:

```text
/dev/shm/smooth_scroll_shm
```

## Requirements

Build and install the effect on the same distribution and Plasma/KWin version
that will run it. KWin effects are native plugins, so the KWin development
headers and libraries should match the target KWin ABI.

You need:

- a C++20 compiler
- CMake 3.25 or newer
- Ninja
- Extra CMake Modules
- Qt 6 development files for Core, DBus, Gui, Quick, and Widgets
- KDE Frameworks 6 development files for CoreAddons, Config, and WindowSystem
- KWin development headers and libraries

Install the matching packages for your distribution:

Debian/Ubuntu:

```bash
sudo apt install build-essential cmake ninja-build extra-cmake-modules \
  qt6-base-dev qt6-declarative-dev \
  libkf6coreaddons-dev libkf6config-dev libkf6windowsystem-dev \
  libdrm-dev kwin-dev
```

Fedora:

```bash
sudo dnf install gcc-c++ cmake ninja-build extra-cmake-modules libdrm-devel \
  'cmake(Qt6Core)' 'cmake(Qt6DBus)' 'cmake(Qt6Gui)' \
  'cmake(Qt6Quick)' 'cmake(Qt6Widgets)' \
  'cmake(KF6CoreAddons)' 'cmake(KF6Config)' \
  'cmake(KF6WindowSystem)' 'cmake(KWin)'
```

Arch Linux:

```bash
sudo pacman -S --needed base-devel cmake ninja extra-cmake-modules \
  qt6-base qt6-declarative \
  kcoreaddons kconfig kwindowsystem \
  libdrm kwin
```

openSUSE:

```bash
sudo zypper install gcc-c++ cmake ninja extra-cmake-modules libdrm-devel \
  'cmake(Qt6Core)' 'cmake(Qt6DBus)' 'cmake(Qt6Gui)' \
  'cmake(Qt6Quick)' 'cmake(Qt6Widgets)' \
  'cmake(KF6CoreAddons)' 'cmake(KF6Config)' \
  'cmake(KF6WindowSystem)' 'cmake(KWin)'
```

If your package manager cannot resolve one of the names above, search for the
package that provides the corresponding CMake package, for example
`Qt6QuickConfig.cmake`, `KF6WindowSystemConfig.cmake`, or `KWinConfig.cmake`.

`qdbus6` is not required to compile the effect, but it is used by the command
line loading example and by `smooth-scroll-kde-rule`. If that command is
missing, install the package in your distribution that provides `qdbus6`.

## Build

```bash
cd extensions/kde
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo
cmake --build build
```

## Install

Install the KWin plugin and the rule picker:

```bash
sudo cmake --install build
```

Then log out and log back in, or restart KWin, so the running KWin process loads
the newly installed plugin. After that, enable the effect in System Settings >
Window Management > Desktop Effects. The plugin id is `kwin_smooth_scroll`.

For command-line loading during development:

```bash
qdbus6 org.kde.KWin /Effects org.kde.kwin.Effects.loadEffect kwin_smooth_scroll
```

## Configure

The effect works with built-in defaults. To customize it, create:

```text
~/.config/smooth-scroll-kde-effect/config.json
```

Start from the example file:

```bash
mkdir -p ~/.config/smooth-scroll-kde-effect
cp config.example.json ~/.config/smooth-scroll-kde-effect/config.json
```

The effect reloads the file when it changes.

`dot`, `arrow`, and `passthrough` control the pointer-side indicators.

`scroll.poll_interval_ms` controls how often the effect samples IPC and window
state. The default is 4ms for lower indicator latency. Increasing it reduces
compositor-thread wakeups, but makes the indicators feel less responsive.

## Force-Passthrough Rules

Do not start by editing `force_passthrough_rules` manually. The easiest and most
reliable workflow is to let KWin identify the window for you:

```bash
smooth-scroll-kde-rule pick
```

The command waits three seconds. Move the pointer over the target window before
the countdown ends. The effect reads KWin's app/window ids and adds or updates
the matching rule in:

```text
~/.config/smooth-scroll-kde-effect/config.json
```

Useful commands:

```bash
smooth-scroll-kde-rule preview
smooth-scroll-kde-rule info
smooth-scroll-kde-rule pick --disable
smooth-scroll-kde-rule pick --title
smooth-scroll-kde-rule pick --delay 5
```

- `preview` prints the rule without writing it.
- `info` prints the window ids KWin sees under the pointer.
- `pick --disable` writes `force_passthrough=false` for the selected app/window.
- `pick --title` creates a title-specific override under the app rule.
- `pick --delay 5` gives you five seconds to move the pointer.

Use `--title` only when one window title should behave differently from the rest
of the same application.

For manual edits, `force_passthrough_rules` accepts application/window rules.
`app` matches KWin's `desktopFileName()` first and also falls back to the window
class. Use `class` only when you need to match the window class directly.

## Troubleshooting

If `smooth-scroll-kde-rule` reports that `org.smooth_scroll.KWinEffect` is not
available, the running KWin process has not loaded this effect yet. Enable the
effect in System Settings, then log out and back in or restart KWin.

If the indicators appear but the daemon does not react to passthrough changes,
check that the Smooth Scroll daemon is running and has created:

```text
/dev/shm/smooth_scroll_shm
```
