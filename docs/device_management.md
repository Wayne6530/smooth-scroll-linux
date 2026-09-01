# Device management and Session lifecycle

The daemon separates device discovery from the latency-sensitive input path. This document records the current
ownership, concurrency, and lifecycle contracts that must be preserved when the implementation changes.

## Ownership

- `DeviceManager::acquireSessionDevices()` is a blocking acquisition state machine. It subscribes to udev before
  taking the first device snapshot, then performs every required rescan internally until it has a stable set of
  already-open, move-only mouse and keyboard handles, shutdown is requested, or a fatal error occurs.
- During acquisition, `DeviceManager` consumes the udev socket synchronously. Once the device set is stable, the
  same socket is moved into a low-priority `DeviceMonitor` worker owned by the returned Session resources. Events
  arriving during that ownership transfer remain queued on the socket, so there is no unmonitored gap.
- `VirtualDevice` is owned by `main`. It stays alive across Session restarts and is rebuilt only between Sessions
  when a newly selected mouse requires capabilities that the current uinput device does not expose.
- `Session` takes ownership of one mouse handle, the current keyboard handles, and the running `DeviceMonitor`.
  A successfully constructed Session has already waited for every key exposed by the physical mouse event node
  to be released and grabbed the mouse. Destroying it ungrabs the mouse and disconnects IPC state. A graceful
  restart first reaches a physical- and virtual-key-neutral frame; `VirtualDevice` therefore remains only the
  persistent uinput capability and output transport and does not track key state.
- `main` coordinates completed Session lifetimes, but it does not retry failed discovery attempts or interpret
  device-change reasons. `DeviceMonitor` sends the active Session only an `eventfd` restart signal.

The real-time loop never polls udev, enumerates devices, opens a new device, or communicates with the monitor
thread through a lock. Its only added work is checking the monitor notification fd and the process shutdown fd in
the same `select()` call used for mouse and keyboard input. The monitor worker inherits a blocked
`SIGINT`/`SIGTERM` mask, so process shutdown is always handled by the main thread that owns this blocking input
loop.

All acquisition waits are event-driven. With no compatible mouse, acquisition waits indefinitely for udev or
shutdown. One compatible mouse is selected immediately. With multiple candidates, acquisition waits indefinitely
for the first candidate that emits `EV_REL`; if removals leave one candidate, that candidate is selected
immediately. There is no active-detection timeout or timed discovery retry. A signal-safe write to the shutdown
`eventfd` wakes every indefinite wait.

## Hotplug behavior

Each `add` event is classified immediately. Virtual devices, excluded models, and event nodes that are neither a
compatible mouse nor a relevant keyboard are ignored. A relevant event starts a sliding
`device_event_debounce_milliseconds` interval; further relevant event nodes extend the same interval. This
coalesces the multiple `/dev/input/event*` nodes commonly created by a composite USB or wireless device without
tracking physical-device groups or paths.

After the interval during acquisition, the current snapshot is discarded and the same function enumerates again.
After acquisition, the monitor sends one restart notification and exits. Because acquisition and runtime
monitoring share one udev subscription, an event that crosses the handoff boundary remains queued and cannot be
lost; it can still cause one harmless restart if it arrives immediately after the final snapshot.

`remove` events never request a restart:

- loss of the active mouse is reported directly by libevdev as `-ENODEV`, which ends the Session;
- loss of a keyboard is handled inside the Session by removing that keyboard handle and updating passthrough
  state;
- loss of any inactive device has no effect on the active input path.

An `add` followed by an immediate `remove` may still request one restart. Unstable or unrelated device models are
outside the monitor's recovery policy and can be excluded with `ignored_devices`.

A manager-requested restart is graceful. The Session latches the request and continues through the normal input
path until the physical mouse has no pressed keys and the pending output frame is empty. The event-transformation
contract guarantees that no virtual key can remain pressed at this boundary: passthrough presses have matching
releases, handled buttons do not create a virtual press, and replayed clicks emit their press and release together.
Session therefore does not separately track or query virtual key state. It then stops the smoother and returns
without a timeout or synthetic releases, so every forwarded press and release remains paired while the physical
mouse is still grabbed. The persistent uinput device survives this restart.

Loss of the active mouse, a fatal input/output error, failure to construct a Session, and process shutdown are
non-graceful paths. They never preserve the uinput device for a later Session. When an active Session returns one
of these results, `main` destroys the uinput device before destroying the Session; if Session initialization
fails, the partially constructed Session unwinds before `main` resets the uinput device. A later acquisition
creates a fresh uinput device and starts a new virtual-device lifecycle.

## Device exclusions

`ignored_devices` is an optional array of device-model tables. A missing or empty array keeps automatic discovery
enabled for every compatible mouse and keyboard. Every table requires an exact 16-bit `vendor_id` and
`product_id` pair; no other matching fields are accepted.

The Web configurator can fill these IDs from the browser's WebHID device chooser. It reads identity and collection
metadata only; it never opens the selected device or exchanges reports with it. Browser security can hide a
standard-only mouse or keyboard, while gaming devices with a vendor-defined configuration interface are commonly
visible. A hidden model can still be entered directly in TOML.

When a model matches, all of its event nodes are ignored consistently during hotplug classification, active-mouse
selection, and keyboard enumeration. This naturally covers composite HID devices that expose separate mouse,
keyboard, and consumer-control event nodes with the same model ID. The daemon's own `Virtual Smooth Mouse` is
always ignored independently of this list.

Example exclusion:

```toml
device_event_debounce_milliseconds = 500
ignored_devices = [
  { vendor_id = 0x046d, product_id = 0xc547 },
]
```

Every relevant keyboard or mouse addition requests the same Session restart. The next acquisition does not prefer
the previous mouse: one candidate is selected immediately, while multiple candidates require fresh `EV_REL`
activity. This keeps hotplug behavior independent of state retained by `main`.

## Remapped mouse buttons

If a mouse and keyboard share one event node, Session forwards all of that node's key capabilities through uinput
and applies `keyboard_braking_keys` and `keyboard_passthrough_keys` to its keyboard-like key events. This includes
`KEY_*` events produced when a mouse driver maps a physical mouse button to a keyboard key. The same mapped key has
the same semantics when the driver exposes it through a separate keyboard event node.

Pointer button codes from `BTN_MOUSE` through `BTN_TASK` always retain mouse-button semantics, regardless of their
event node, and are rejected in both keyboard key lists. Keyboard-like keys on the grabbed mouse node participate
in the same neutral restart boundary. Separate keyboard event nodes are observed but never grabbed or forwarded,
so they do not participate in that boundary.
