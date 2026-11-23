# Using Smooth Scroll on KDE Plasma

The Smooth Scroll desktop effect shows scrolling indicators near your pointer and
selects a profile for the window under it. Use the
[Web Configurator](https://wayne6530.github.io/smooth-scroll-linux/config.html)
to configure scrolling profiles and note their numeric IDs. In KDE System Settings,
choose which applications, windows and desktop effects use each ID.

## Install and open settings

Install the daemon using the [project guide](../../README.md). The desktop effect
is built from source on the Plasma 6 machine where you will use it. It has been
tested on Wayland with KWin 6.6.6 and 6.7.5.

<details>
<summary>Install the build dependencies for your distribution</summary>

Ubuntu 26.04 / Debian with Plasma 6:

```bash
sudo apt install build-essential cmake ninja-build extra-cmake-modules \
  qt6-base-dev qt6-declarative-dev \
  libkf6coreaddons-dev libkf6config-dev libkf6kcmutils-dev libkf6windowsystem-dev \
  libdrm-dev kwin-dev
```

Fedora:

```bash
sudo dnf install gcc-c++ cmake ninja-build extra-cmake-modules libdrm-devel \
  'cmake(Qt6Core)' 'cmake(Qt6DBus)' 'cmake(Qt6Gui)' \
  'cmake(Qt6Quick)' 'cmake(Qt6Widgets)' \
  'cmake(KF6CoreAddons)' 'cmake(KF6Config)' \
  'cmake(KF6KCMUtils)' 'cmake(KF6WindowSystem)' 'cmake(KWin)'
```

Arch Linux:

```bash
sudo pacman -S --needed base-devel cmake ninja extra-cmake-modules \
  qt6-base qt6-declarative \
  kcoreaddons kconfig kcmutils kwindowsystem \
  libdrm vulkan-headers kwin
```

openSUSE:

```bash
sudo zypper install gcc-c++ cmake ninja extra-cmake-modules libdrm-devel \
  'cmake(Qt6Core)' 'cmake(Qt6DBus)' 'cmake(Qt6Gui)' \
  'cmake(Qt6Quick)' 'cmake(Qt6Widgets)' \
  'cmake(KF6CoreAddons)' 'cmake(KF6Config)' \
  'cmake(KF6KCMUtils)' 'cmake(KF6WindowSystem)' 'cmake(KWin)'
```

</details>

1. Download and extract the **Source code** archive from the
   [GitHub Release](https://github.com/Wayne6530/smooth-scroll-linux/releases)
   matching your daemon. From the root of the extracted repository, run:

   ```sh
   cmake -S extensions/kde -B build/kde -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=/usr
   cmake --build build/kde
   sudo cmake --install build/kde
   ```

2. Log out and back in so KWin loads the installed version.
3. Open **System Settings → Window Management → Desktop Effects**, enable
   **Smooth Scroll**, and click **Apply**. Click the configuration button beside
   **Smooth Scroll** to open its settings.

If the effect stops loading after a KWin upgrade, rebuild and install it against
the updated KWin development packages, then log out and back in.

## Use a profile for an application

1. Open the target application and leave its main window visible.
2. Under **Application rules**, click **Select window…**, then click the target
   window. Press **Esc** to cancel. Picking the same application again selects
   its existing rule.
3. Set **Profile ID** in the application's row and click **Apply**.
   The rule applies when the pointer is over that application's windows.

<a id="compatibility-passthrough-rules"></a>

For compatibility passthrough, choose the ID of your passthrough profile.
The daemon's supplied configuration uses **1**. If you disable a rule using its
checkbox, the application uses **Default application profile** (initially **0**).

## Use a different profile for particular window titles

For example, to give browser windows with “Maps” in their title a different profile:

1. Select the browser's rule and click **Add title…**.
2. Choose **Contains** and enter `Maps` in **Title text**. To copy an actual
   title, click **From window…**, select a window of the same application, and
   keep the part of its title you want to match. Enable **Ignore case** if needed.
3. Set **Profile ID**, click **Save** to return to the rule list, then click
   **Apply** to activate the change.

The first enabled title exception that matches wins. Use **Up** and **Down** to
give a more specific exception priority. Other titles use the application's profile.

## Choose a profile for the desktop or an effect

Under **Desktop and effects**, set **Shell profile ID** for the desktop, panels
and other non-application targets (initially **1**). To give the application
launcher a different profile, choose **Applet popup** under **Window type**,
check **Override Shell profile**, set the ID, and click **Apply**.

Use **Active effect** to assign a profile to **Overview** or another supported
effect. While that effect is active, its profile takes priority over the window
beneath the pointer.

## Adjust scrolling behavior and indicators

Open **Behavior** to choose whether inertia stops when the pointer leaves a
window. Open **Indicators** to adjust the appearance of the inertia dot,
Free Spin ring, Drag View arrow, Auto Scroll marker or compatibility indicator.
Click **Apply** to use your changes.

**Reset** discards edits made since the last save. **Defaults** restores the
built-in values for the whole effect, including an empty application rule list.
Click **Apply** to save those defaults.

## Check which profile is active

Keep **Application rules** visible and move the pointer over the target.
**Requested** is the profile ID selected by the effect. **Active** is the ID
the daemon is using.

- If **Requested** is unexpected, check the rule's checkbox and the order of its
  title exceptions. For the desktop or overview, check **Desktop and effects**.
- If **Requested** and **Active** differ, finish the current scrolling action
  and scroll again. If they still differ, check that the requested ID exists
  in the configuration applied to the daemon.
- If window selection is unavailable, check that **Smooth Scroll** is enabled in
  **Desktop Effects**. If the daemon or input device is unavailable, check that
  the service is running and move or scroll the mouse you want to use.
