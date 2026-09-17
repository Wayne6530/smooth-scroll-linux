# Smooth Scroll IPC Companion

This GNOME Shell extension adds visual indicators and per-window
compatibility-passthrough rules for Smooth Scroll Linux. It communicates with
the daemon through the shared-memory IPC file:

```text
/dev/shm/smooth_scroll_shm
```

While this extension is enabled, avoid running other tools that write Smooth
Scroll IPC control fields, including the bundled CLI control commands. Competing
writes to `scroll_id` or `compatibility_passthrough_requested` can make
compatibility-passthrough or braking behavior unpredictable.

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
- Applies `compatibility_passthrough` from per-window rules based on the window
  under the pointer.

![GNOME pointer-side indicators](assets/pointer-side-indicators.svg)

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
ready and the warning color used while a special mode is still active.
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

Smooth scrolling is only applied inside regular application windows. When the
pointer is on GNOME Desktop, GNOME Dock / Ubuntu Dock / Dash to Dock, the GNOME
overview, workspace switcher, menus, or other Shell UI, the extension requests
compatibility passthrough. The full X appears when it is ready; an active Free
Spin, Drag View, or Auto Scroll interaction continues first with its indicator
recolored to the configured passthrough color.

Use `compatibility_passthrough_rules` when a specific application window should
use compatibility passthrough. The old `force_passthrough_rules` and
`force_passthrough` names are not read or migrated. Each app rule must include:

- `app`: exact app id from Looking Glass, for example
  `firefox_firefox.desktop`.
- `compatibility_passthrough`: default compatibility passthrough behavior for
  this app.
- `titles`: optional title-specific overrides for this app.

Each item under `titles` uses:

- `title`: JavaScript regular expression matched against the window title.
- `compatibility_passthrough`: compatibility passthrough behavior when the title
  matches.

The first app rule whose `app` matches is used. Inside that rule, title rules
are checked in order; the first matching title rule overrides the app default.
If no app rule matches the current window, smooth scrolling stays enabled for
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
  "compatibility_passthrough": true
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
  "compatibility_passthrough": false,
  "titles": [
    {
      "title": "Google Docs",
      "compatibility_passthrough": true
    }
  ]
}
```

This means Edge normally does not use compatibility passthrough, but Edge
windows whose title contains `Google Docs` do.

Put app rules inside `compatibility_passthrough_rules`:

```json
{
  "compatibility_passthrough_rules": [
    {
      "app": "firefox_firefox.desktop",
      "compatibility_passthrough": true
    }
  ]
}
```

After adding or editing rules, save `config.json`. The extension reloads it
automatically. Move the pointer over the target window; if compatibility
passthrough is ready, the full X appears near the pointer. If Free Spin, Drag
View, or Auto Scroll is still active, its current indicator turns red to show
that the request is waiting for the special mode to end.
