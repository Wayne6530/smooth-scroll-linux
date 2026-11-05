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
{"pid":12345,"connected":true,"keyboard_passthrough":false,"requested_profile_id":1,"active_profile_id":0,"drag_view":false,"free_spin":true,"auto_scroll":false,"auto_scroll_horizontal_enabled":false,"auto_scroll_vertical_enabled":true,"auto_scroll_offset_x":0,"auto_scroll_offset_y":0,"horizontal":false,"direction":"positive","speed":0}
```

Here profile 1 has been requested, but profile 0 remains active while Free Spin
is held. `requested_profile_id` records the target; `active_profile_id` records
the actual selection.

`ss-stop` brakes ordinary inertia, re-anchors a held Auto Scroll gesture, or
exits latched Auto Scroll.

```bash
ss-stop
```

`ss-profile <id>` requests a configured profile:

```bash
ss-profile 1    # Request compatibility passthrough
ss-profile 0    # Return to the default profile
```

The request waits for special interactions and all keys in the union of profile
passthrough bindings to be released. Ordinary inertia stops on switching.
Requests are processed on input/tick wakeups; idle confirmation can be delayed.
An unknown ID leaves the active profile unchanged. Use `ss-status` to observe it.


IDs follow configuration order: top-level options are profile 0 and successive
`[[profiles]]` tables are 1, 2, and so on. The default config includes profile 1
for compatibility passthrough. `ss-profile --help` prints usage without requiring
a running daemon. A successful request write does not mean the switch has finished.

The daemon must be running and expose IPC v3 at `/dev/shm/smooth_scroll_shm`.
Avoid competing profile writes from CLI commands while a desktop plugin manages
selection from the window under the pointer.
