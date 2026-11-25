# Using Smooth Scroll on GNOME

The Smooth Scroll extension shows scrolling indicators near your pointer and
selects a scrolling profile when you move the pointer over an application. Use
the [Web Configurator](https://wayne6530.github.io/smooth-scroll-linux/config.html)
to configure profiles and note their numeric IDs. Use the GNOME extension's
preferences to choose where each ID applies.

## Install and open preferences

The extension supports GNOME Shell 45–50 and works with the Smooth Scroll daemon.
If you need to install the daemon first, follow the [project guide](../../README.md).

1. Open the [GitHub Release](https://github.com/Wayne6530/smooth-scroll-linux/releases)
   matching your daemon version. Under **Assets**, download
   `smooth-scroll@wayne6530.shell-extension.zip`. In the folder containing the
   downloaded ZIP, run:

   ```sh
   gnome-extensions install --force ./smooth-scroll@wayne6530.shell-extension.zip
   ```

2. Log out and log back in so GNOME loads the extension.
3. Open **Extensions**, enable **Smooth Scroll**, and open its
   preferences. If the extension is already enabled, you can open preferences
   directly with `gnome-extensions prefs smooth-scroll@wayne6530`.

## Use a profile for an application

1. Open the application you want to configure and leave its window visible.
2. In preferences, open **Application rules** and click **+** beside **Applications**.
   Preferences will hide so you can click the target window. Press **Esc** to cancel.
3. Set **Default Profile ID** to the ID you chose in the Web Configurator and click
   **Save**. The rule applies whenever the pointer is over a window of that application.

GNOME identifies the application from the selected window. To change a saved rule,
use its edit button in **Applications**, even when the application is closed.
Use the switch beside a saved rule to enable or disable it immediately.
Under **Default profiles**, set **Default Profile ID** for applications without
an enabled rule (initially **0**). For the desktop, dock,
overview and Shell menus, set **Shell Profile ID** on the same page (default **1**).

## Use a different profile for particular window titles

For example, to use a different profile for a browser window whose title contains
“Maps”, open the browser's application rule:

1. Click **+** beside **Window title exceptions**. Choose **Contains** and enter
   `Maps` in **Title text**. To copy an actual title, click **Get title from a window** beside the text,
   click a window of the same application, and keep the part you want to match.
2. Set **Profile ID** for these windows and click **Save**.
3. Click **Save** in the application rule to apply your changes. **Cancel** discards
   the draft, including any exceptions you edited.

Matching uses the window title shown by the application. Choose **Equals**,
**Starts with** or **Ends with** for a more specific match. Ordinary text is
case-sensitive unless you enable **Ignore case**. If multiple exceptions match,
the first enabled one wins. Toggle individual exceptions in the exception list
and click **Save** to apply your changes. Use the arrow buttons to move a more specific
exception above a broader one. Other titles use the application's default profile.

## Adjust scrolling behavior and indicators

Open **Behavior** to control whether inertia stops when the pointer leaves a
window. To hide the direct mode on-screen display, turn off **Show on-screen feedback when direct mode changes**.
Open **Indicators** to adjust the appearance of the inertia dot, Free
Spin ring, Drag View arrow, Auto Scroll marker or compatibility indicator.
For example, change the Auto Scroll size to resize its marker (default **24 px**).
Changes on these pages apply immediately. Use the reset button beside a group
heading to restore that group's defaults.

## Check which profile is active

Open **Application rules** and move the pointer over the window you want to check.
At the top of the page, **Requested** is the ID selected by the extension and
**Active** is the ID in use by the daemon.

- If **Requested** is unexpected, check that the application rule is enabled and
  that its first matching title exception uses the intended ID.
- If you see **Not yet applied**, finish the current scrolling action and try
  scrolling again. If the IDs still differ, check the ID in the Web Configurator
  and update the rule if the profile's ID has changed.
- If window selection is unavailable, check that the extension is enabled.
  If GNOME cannot identify the application, try selecting its main window.
