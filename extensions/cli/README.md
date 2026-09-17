# CLI Extension

The CLI extension provides small terminal utilities for monitoring and controlling a running `smooth-scroll` daemon through the shared-memory IPC protocol.

## Build

Before building, change to this extension directory:

```bash
cd extensions/cli
```

Then build the tools:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
```

## Install

Run the install command from this same directory after the build finishes.

Install to `/usr/local`:

```bash
sudo cmake --install build
```

Install to `/usr` instead:

```bash
sudo cmake --install build --prefix /usr
```

## Usage

`ss-status` streams daemon state as JSON Lines:

```bash
ss-status
```

Example output:

```json
{"pid":12345,"connected":true,"keyboard_passthrough":false,"compatibility_passthrough_requested":false,"drag_view":false,"free_spin":false,"auto_scroll":true,"auto_scroll_horizontal_enabled":true,"auto_scroll_vertical_enabled":true,"auto_scroll_offset_x":12,"auto_scroll_offset_y":-24,"horizontal":false,"direction":"negative","speed":0}
```

`ss-stop` brakes ordinary inertia and either re-anchors a held Auto Scroll gesture or exits latched Auto Scroll:

```bash
ss-stop
```

`ss-passthrough` toggles or sets compatibility-passthrough mode. The request
takes effect after any active Free Spin, Drag View, or Auto Scroll interaction
finishes:

```bash
ss-passthrough       # Toggle the compatibility-passthrough request
ss-passthrough 1     # Request compatibility passthrough
ss-passthrough off   # Clear the compatibility-passthrough request
```

The daemon must be running before these commands can connect to `/dev/shm/smooth_scroll_shm`.
