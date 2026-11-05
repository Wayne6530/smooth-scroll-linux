# Smooth Scroll IPC Companion

This GNOME Shell extension adds visual indicators and per-window
profile rules for Smooth Scroll Linux. It communicates with
the daemon through the shared-memory IPC file:

```text
/dev/shm/smooth_scroll_shm
```

While this extension is enabled, avoid running other tools that write Smooth
Scroll IPC control fields, including the bundled CLI control commands. Competing
writes to `scroll_id` or `requested_profile_id` can make
profile selection or braking behavior unpredictable.

## Features

- Draws a small dot near the pointer while inertial scrolling is active.
- Draws a four-way arrow near the pointer while Drag View is active.
- Draws a stationary circled four-triangle origin marker during Auto Scroll,
  with a blue dot that follows the IPC pointer offset.
- Draws an X near the pointer while Compatibility Passthrough is ready. While
  an existing special mode is finishing, its current indicator turns red.
- Draws an outer ring while Free Spin is active, including at zero speed.
- Stops inertial scrolling when the pointer leaves the window where scrolling
  started.
- Applies numeric `profile_id` from per-window rules based on the window
  under the pointer.

## Install

Before installing, change to this extension directory:

```bash
cd extensions/gnome
```

Then install the extension and example config:

```bash
mkdir -p ~/.local/share/gnome-shell/extensions ~/.config/smooth-scroll-gnome-extension \
  && cp -r smooth-scroll@wayne6530 ~/.local/share/gnome-shell/extensions/ \
  && cp config.example.json ~/.config/smooth-scroll-gnome-extension/config.json
```

Make sure GNOME's global user-extension switch allows locally installed
extensions:

```bash
gsettings set org.gnome.shell disable-user-extensions false
```

On Wayland, log out and log back in after copying the extension so GNOME Shell
can discover it. Then enable the extension:

```bash
gnome-extensions enable smooth-scroll@wayne6530
```

On X11, `Alt+F2`, then `r`, then Enter is usually enough before enabling.

If the extension still does not start, check both switches:

```bash
gsettings get org.gnome.shell disable-user-extensions
gnome-extensions info smooth-scroll@wayne6530
```

`disable-user-extensions` must be `false`, and the extension must be listed as
enabled.

## Configure

The extension reads its own config file:

```text
~/.config/smooth-scroll-gnome-extension/config.json
```

The install command above copies the example config to this path. The extension
reloads the config when the file changes.

Under `dot`, `offset_x` and `offset_y` place the inertial-scroll dot center
relative to the pointer, not the top-left corner of the indicator.

`arrow` controls the Drag View arrow independently from `dot`. `"enabled":
false` disables the arrow. `offset_x`, `offset_y`, `size`, `color`, and `alpha`
control placement and appearance; `padding_scale`, `head_size_scale`,
`line_width_scale`, and `head_width_scale` control the arrow geometry.

`free_spin` controls the ring drawn around the current indicator while Free
Spin is active. `color` and `alpha` control its normal appearance;
`ring_gap` is the transparent gap outside the current indicator and
`ring_width` is the stroke width. The ring remains visible by itself when Free
Spin reaches zero speed. It uses the `dot` placement when no other indicator is
active.

`auto_scroll` controls the Auto Scroll origin marker independently from the
other indicators. The circle and four unconnected triangles remain fixed while
the blue dot follows `auto_scroll_offset` from IPC. `offset_x`, `offset_y`,
`size`, `color`, `dot_color`, `dot_size`, and `alpha` control its placement and
appearance. The blue dot uses the IPC offset directly and may move outside the
origin circle; the daemon limits that offset at the displacement corresponding
to the configured maximum Auto Scroll speed.

`passthrough` controls the full X drawn while compatibility passthrough is
active (profile 1), and the warning color used when a profile change is pending
during a special mode.
`"enabled": false` disables both behaviors. `offset_x` and `offset_y` control
the full X placement relative to the pointer. `size`, `padding_scale`,
`min_padding`, `line_width_scale`, `min_line_width`, and `alpha` control the X;
`color` controls both the X and the warning recolor. Drag View's arrow and the
Free Spin dot/ring use this color while pending. Auto Scroll keeps its neutral
origin frame and recolors its moving dot.

`poll_interval_ms` controls how often the extension reads IPC state. Pointer
position updates are event-driven, so the indicator follows mouse movement
without waiting for the next IPC poll.

`pointer_fallback` controls pointer sampling while an indicator is visible. The
extension uses pointer events immediately when GNOME Shell receives them; if no
pointer event arrives for `event_fresh_ms`, it samples `global.get_pointer()`
every `interval_ms` to keep indicators responsive over regular application
windows. The defaults, `interval_ms = 4` and `event_fresh_ms = 6`, are tuned for
120 Hz displays.

## Window Rules

When the pointer is on GNOME Desktop, GNOME Dock / Ubuntu Dock / Dash to Dock, the GNOME
overview, workspace switcher, menus, or other Shell UI, the extension requests
`shell_profile_id` (default 1, compatibility passthrough). The full X appears
when profile 1 is active. If a different profile is requested during Free Spin,
Drag View or Auto Scroll, that interaction continues with its indicator recolored
to the configured passthrough color while the request is pending.

Use `profile_rules` to select a profile for application windows. App rules use these fields:

- `app`: exact app id from Looking Glass, for example
  `firefox_firefox.desktop`.
- `profile_id`: profile selected for this app (default 0).
- `titles`: optional title-specific overrides for this app.

Each item under `titles` uses:

- `title`: JavaScript regular expression matched against the window title.
- `profile_id`: profile selected when the title matches; omitted values use
  the app rule’s profile.

The first app rule whose `app` matches is used. Inside that rule, title rules
are checked in order; the first matching title rule overrides the app default.
If no app rule matches the current window, profile 0 is requested for
that window.

### Finding Window Values

Use GNOME Shell Looking Glass:

1. Press `Alt+F2`, type `lg`, and press Enter.
2. Open the Windows tab.
3. Find the target window by title.
4. Copy the value after `app:` into the rule's `app` field.

Looking Glass shows entries like:

```text
Example Page - Mozilla Firefox
wmclass: firefox
app: firefox_firefox.desktop
```

Use the `app:` value:

```json
{
  "app": "firefox_firefox.desktop",
  "profile_id": 1
}
```

`wmclass` is shown by Looking Glass to help you identify the window, but this
extension's rule system only uses `app`.

Use `titles` when only some windows inside an app need different behavior. The
`title` value is a regular expression, so it can match a stable part of the
window title instead of the whole title:

```json
{
  "app": "microsoft-edge.desktop",
  "profile_id": 0,
  "titles": [
    {
      "title": "Google Docs",
      "profile_id": 1
    }
  ]
}
```

This means Edge normally does not use compatibility passthrough, but Edge
windows whose title contains `Google Docs` do.

Put app rules inside `profile_rules`:

```json
{
  "profile_rules": [
    {
      "app": "firefox_firefox.desktop",
      "profile_id": 1
    }
  ]
}
```

Set `enabled: false` on an app or title rule to skip it.

After adding or editing rules, save `config.json`. The extension reloads it
automatically. Move the pointer over the target window; if compatibility
passthrough (profile 1) is active, the full X appears near the pointer. If a
profile change is pending during Free Spin, Drag View or Auto Scroll, its
indicator uses the configured passthrough color (red by default).

Profile IDs correspond to the daemon configuration: top-level options are 0,
and successive `[[profiles]]` tables are 1, 2, and so on. The default configuration
reserves profile 1 for compatibility passthrough. Other IDs select your custom
profiles; they do not display the compatibility X. Set `shell_profile_id` at the
JSON root to choose a different profile for Shell UI.

Requests wait for Free Spin, Drag View and Auto Scroll to finish and for all
keyboard passthrough keys configured in any profile to be released. Ordinary
inertia stops on switching. An idle daemon may apply the request on the next
input. Ensure the requested ID exists; an unknown ID leaves the active profile
unchanged. The plugin and daemon must use IPC v3.
