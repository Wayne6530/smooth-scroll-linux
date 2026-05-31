# Smooth Scroll Extensions

Extensions are optional components that communicate with the `smooth-scroll` daemon through the shared-memory IPC protocol documented in `docs/ipc_protocol.md`. The canonical C++ protocol contract lives in `include/smooth_scroll/ipc_protocol.h`.

The daemon package only installs the service, daemon binary, and default configuration. Extensions are intentionally built and installed separately so users can choose the integrations they need.

Current layout:

```text
extensions/
  cli/      Terminal utilities built on top of the IPC protocol
```

Future desktop-environment integrations should live beside `cli/`, for example:

```text
extensions/
  gnome/    GNOME Shell extension
  kde/      KDE Plasma widget or service
```

Each extension should keep its own build files, install instructions, and user-facing README in its directory.
