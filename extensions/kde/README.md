# Smooth Scroll KWin Effect

This KWin effect adds KDE Plasma integration for Smooth Scroll Linux. Use it
when the Smooth Scroll daemon is already installed and running, and you want
session-aware behavior in KDE Plasma.

It provides:

- configurable profile selection when the pointer is not over a regular application
  window
- per-application profile rules
- inertial scrolling stop when the pointer leaves the window where scrolling
  started
- pointer-side indicators for inertial scrolling, Free Spin, Drag View, Auto
  Scroll, and compatibility passthrough

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
  libdrm vulkan-headers kwin
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
ctest --test-dir build --output-on-failure
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

`dot`, `free_spin`, `arrow`, `auto_scroll`, and `passthrough` control the
pointer-side indicators. `shell_profile_id` is the fallback profile for non-application
targets and defaults to 1. [Window type and effect mappings](#window-type-and-effect-profiles)
can override it.

`free_spin` controls the ring drawn around the current indicator while Free
Spin is active. `color` and `alpha` control its normal appearance; `ring_gap`
is the transparent gap outside the current indicator and `ring_width` is the
stroke width. The ring remains visible by itself when Free Spin reaches zero
speed. It uses the `dot` placement when no other indicator is active.

`auto_scroll` controls the Auto Scroll origin marker independently from the
other indicators. The circle and four unconnected triangles remain fixed while
the blue dot follows `auto_scroll_offset` from IPC. `offset_x`, `offset_y`,
`size`, `color`, `dot_color`, `dot_size`, and `alpha` control its placement and
appearance. The blue dot uses the IPC offset directly and may move outside the
origin circle; the daemon limits that offset at the displacement corresponding
to the configured maximum Auto Scroll speed.

`scroll.poll_interval_ms` controls how often the effect samples IPC and window
state. The default is 4ms for lower indicator latency. Increasing it reduces
compositor-thread wakeups, but makes the indicators feel less responsive.

The Free Spin ring remains visible while Free Spin is active, including when
its scroll speed reaches zero. If a different profile is requested while
Free Spin, Drag View, or Auto Scroll is active, the current special-mode
indicator uses the configured passthrough color while the request is pending.
The normal full-size X appears only when profile 1 (compatibility passthrough)
is active. Auto Scroll keeps its
neutral origin frame and recolors only its moving dot.

`passthrough.enabled = false` disables both the full X and the pending warning
color. `passthrough.color` controls both appearances.

Keyboard passthrough does not display the compatibility indicator.

## Profile Rules

Do not start by editing `profile_rules` manually. The easiest
and most reliable workflow is to let KWin identify the window for you:

```bash
smooth-scroll-kde-rule pick --profile 1
```

The command waits three seconds. Move the pointer over the target window before
the countdown ends. The effect reads KWin's app/window ids and adds or updates
the matching rule in:

```text
~/.config/smooth-scroll-kde-effect/config.json
```

Useful commands:

```bash
smooth-scroll-kde-rule preview --profile 1
smooth-scroll-kde-rule info
smooth-scroll-kde-rule pick --profile 0
smooth-scroll-kde-rule pick --profile 1 --title
smooth-scroll-kde-rule pick --profile 1 --delay 5
```

- `preview` prints the rule without writing it.
- `info` prints the current target and profile selection diagnostics.
- `pick --profile 0` selects the default profile for the selected
  app/window.
- `pick --profile 1 --title` creates a title-specific override under the app rule.
- `pick --profile 1 --delay 5` gives you five seconds to move the pointer.

`info` reports the same target used for profile selection, including when a
supported interaction effect overrides the window under the pointer.
`requested_profile_now` is the profile computed at capture time, not confirmation
that the daemon has already switched. `window_type_name`, `interaction_effect`,
and `profile_reason` identify the configuration key and selection reason.

Use `--title` only when one window title should behave differently from the rest
of the same application.

For manual edits, `profile_rules` accepts application/window
rules. `app` matches KWin's `desktopFileName()` first and also falls back to the
window class. Use `class` only when you need to match the window class directly.

Rules and title overrides use numeric `profile_id` values. The daemon's top-level
options are profile 0; successive `[[profiles]]` tables are 1, 2, and so on.
Profile 1 is compatibility passthrough in the default configuration. Use
`--profile 2`, for example, to select your third configured profile.

```json
{
  "shell_profile_id": 1,
  "profile_rules": [
    {
      "app": "firefox",
      "profile_id": 0,
      "titles": [{"title": "Example", "profile_id": 1}]
    }
  ]
}
```

The first matching application rule is used. Within it, the first matching title
regular expression overrides the app profile. An omitted title `profile_id`
uses the app profile; an omitted app `profile_id` is 0. Set `enabled: false` on
an app or title rule to skip it. Without a matching app rule, profile 0 is used.
Title expressions use Qt regular expression syntax. `shell_profile_id` supplies
the fallback outside regular application windows and defaults to 1.

Requests wait for Free Spin, Drag View and Auto Scroll to finish and for all
keyboard passthrough keys configured in any profile to be released. Ordinary
inertia stops on switching. An idle daemon may apply the request on the next
input. The plugin and daemon must use IPC v3.

## Window Type and Effect Profiles

`window_type_profiles` and `effect_profiles` map KWin window types and interaction
effects to daemon profile IDs. Unmapped shell targets use `shell_profile_id`,
which defaults to 1.

For example, to use profile 0 in Plasma applet popups:

```json
{
  "shell_profile_id": 1,
  "window_type_profiles": {"applet_popup": 0},
  "effect_profiles": {"overview": 1, "windowview": 1}
}
```

The `applet_popup` mapping applies to the application launcher, clipboard, volume,
and other applet popups.

Selection order:

1. A supported active interaction effect uses its `effect_profiles` entry,
   or `shell_profile_id` if absent, regardless of the window underneath.
2. Regular application windows and dialogs use application/title rules,
   falling back to profile 0.
3. Other windows use their `window_type_profiles` entry, or `shell_profile_id`.
4. When there is no window under the pointer, use `shell_profile_id`.

Supported window type keys:

- Desktop and panel: `desktop`, `dock`.
- Menus and popups: `applet_popup`, `menu`, `dropdown_menu`, `popup_menu`, `combo_box`.
- Notifications: `tooltip`, `notification`, `critical_notification`, `on_screen_display`.
- Other: `lock_screen`, `input_method`, `outline`, `dnd_icon`, `toolbar`, `splash`,
  `utility`, `unknown`.

Application-owned standalone menus also use window type mappings; they do not
inherit their parent application's profile.

Supported effect keys, in priority order when multiple are active:
`overview`, `windowview`, `desktopgrid`, `presentwindows`,
`presentwindows_current`, `presentwindows_all`. `windowview` is the window overview
(Present Windows). Available effects depend on the KWin version; visual effects
such as `blur` do not participate in profile selection.

Use `smooth-scroll-kde-rule info` to find the current `window_type_name` or
`interaction_effect` and check `profile_reason`. Mapping keys are exact and
case-sensitive.

With `stop_on_pointer_leave_window` enabled, leaving or switching an interaction
effect stops scrolling.

## Troubleshooting

If `smooth-scroll-kde-rule` reports that `org.smooth_scroll.KWinEffect` is not
available, the running KWin process has not loaded this effect yet. Enable the
effect in System Settings, then log out and back in or restart KWin.

If the indicators appear but the daemon does not react to
profile requests, check that the Smooth Scroll daemon is
running and has created:

```text
/dev/shm/smooth_scroll_shm
```

If a request remains pending, check that its ID exists in the daemon configuration
and that no special interaction or keyboard passthrough key is still active.
