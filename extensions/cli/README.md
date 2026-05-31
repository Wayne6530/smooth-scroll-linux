# CLI Extension

The CLI extension provides small terminal utilities for monitoring and controlling a running `smooth-scroll` daemon through the shared-memory IPC protocol.

## Build

Build it separately from the daemon:

```bash
cmake -S extensions/cli -B build-cli -DCMAKE_BUILD_TYPE=Release
cmake --build build-cli --parallel
```

## Install

Install to `/usr/local`:

```bash
sudo cmake --install build-cli
```

Install to `/usr` instead:

```bash
sudo cmake --install build-cli --prefix /usr
```

## Usage

`ss-status` streams daemon state as JSON Lines:

```bash
ss-status
```

Example output:

```json
{"pid":12345,"connected":true,"passthrough":false,"drag_view":false,"free_spin":false,"horizontal":false,"direction":"positive","speed":150}
```

`ss-stop` immediately stops any active inertial scroll:

```bash
ss-stop
```

`ss-passthrough` toggles or sets force-passthrough mode:

```bash
ss-passthrough       # Toggle state
ss-passthrough 1     # Enable passthrough
ss-passthrough off   # Disable passthrough
```

The daemon must be running before these commands can connect to `/dev/shm/smooth_scroll_shm`.
