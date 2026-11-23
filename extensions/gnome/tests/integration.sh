#!/bin/bash
# GNOME 50 headless integration test. Never install into the user's active session.
set -euo pipefail
cd "$(dirname "$0")/.."
export SMOOTH_SCROLL_SOURCE_DIR="$PWD"
export SMOOTH_SCROLL_TEST_DIR
SMOOTH_SCROLL_TEST_DIR=$(mktemp -d /tmp/smooth-scroll-gnome.XXXXXX)
export XDG_CONFIG_HOME="$SMOOTH_SCROLL_TEST_DIR/config"
export XDG_DATA_HOME="$SMOOTH_SCROLL_TEST_DIR/data"
export XDG_CACHE_HOME="$SMOOTH_SCROLL_TEST_DIR/cache"
export XDG_RUNTIME_DIR="$SMOOTH_SCROLL_TEST_DIR/runtime"
export GSETTINGS_BACKEND=keyfile WAYLAND_DISPLAY=smooth-scroll-test
unset DISPLAY WAYLAND_SOCKET
mkdir -p "$XDG_CONFIG_HOME" "$XDG_DATA_HOME/gnome-shell/extensions" "$XDG_DATA_HOME/applications" "$XDG_CACHE_HOME" "$XDG_RUNTIME_DIR"
chmod 700 "$XDG_RUNTIME_DIR"
cp -r smooth-scroll@wayne6530 "$XDG_DATA_HOME/gnome-shell/extensions/"
# Only the temporary test copy exposes an Eval handle. Redirect IPC before loading
# it, so even a running real daemon cannot receive test profile/brake requests.
python3 - "$XDG_DATA_HOME/gnome-shell/extensions/smooth-scroll@wayne6530/extension.js" "$SMOOTH_SCROLL_TEST_DIR/ipc" <<'PY'
from pathlib import Path
import os, struct, sys
path = Path(sys.argv[1])
source = path.read_text()
assert "'/dev/shm/smooth_scroll_shm'" in source
source = source.replace("'/dev/shm/smooth_scroll_shm'", repr(sys.argv[2]))
source = source.replace('    enable() {', '''    enable() {
        global.context.unsafe_mode = true;
        global.smoothScrollTestExtension = this;
        global.smoothScrollTestClutter = Clutter;''')
path.write_text(source)
Path(sys.argv[2]).write_bytes(struct.pack('<8I', 0x53530003, os.getpid(), 1, 0, 0, 0, 0, 0))
PY
cat > "$XDG_DATA_HOME/applications/org.example.SmoothScrollFixture.desktop" <<'DESKTOP'
[Desktop Entry]
Type=Application
Name=Smooth Scroll Fixture
Exec=gjs
Icon=application-x-executable
DESKTOP
export GSETTINGS_SCHEMA_DIR="$XDG_DATA_HOME/gnome-shell/extensions/smooth-scroll@wayne6530/schemas"
glib-compile-schemas --strict "$GSETTINGS_SCHEMA_DIR"
status=0
dbus-run-session -- bash -euo pipefail <<'SESSION' > "$SMOOTH_SCROLL_TEST_DIR/session.log" 2>&1 || status=$?
gsettings set org.gnome.shell enabled-extensions '["smooth-scroll@wayne6530"]'
gsettings set org.gnome.shell disable-user-extensions false
gsettings set org.gnome.desktop.interface enable-animations false
gsettings set org.gnome.desktop.session idle-delay 0
gnome-shell --headless --wayland --no-x11 --virtual-monitor 1280x800 --wayland-display smooth-scroll-test > "$SMOOTH_SCROLL_TEST_DIR/shell.log" 2>&1 &
shell_pid=$!
cleanup() {
    gnome-extensions disable smooth-scroll@wayne6530 || true
    kill "$shell_pid" 2>/dev/null || true
    wait "$shell_pid" || true
}
trap cleanup EXIT
ready=false
for attempt in $(seq 1 80); do
    if test -S "$XDG_RUNTIME_DIR/$WAYLAND_DISPLAY" && gdbus call --session --dest org.gnome.Shell \
        --object-path /org/gnome/Shell/Extensions/SmoothScroll \
        --method org.gnome.Shell.Extensions.SmoothScroll.GetStatus >/dev/null 2>&1; then
        ready=true
        break
    fi
    sleep 0.25
done
if ! "$ready"; then
    cat "$SMOOTH_SCROLL_TEST_DIR/shell.log"
    exit 1
fi
gjs -m "$SMOOTH_SCROLL_SOURCE_DIR/tests/integration.js"
SESSION
if [ "$status" -eq 0 ]; then
    # A successful test must not hide runtime JS errors in the Shell process.
    if rg 'JS ERROR|Gjs-CRITICAL' "$SMOOTH_SCROLL_TEST_DIR/shell.log"; then
        status=1
    fi
fi
if [ "$status" -eq 0 ]; then
    rg 'passed\.' "$SMOOTH_SCROLL_TEST_DIR/session.log"
else
    cat "$SMOOTH_SCROLL_TEST_DIR/session.log" "$SMOOTH_SCROLL_TEST_DIR/shell.log"
fi
printf 'Integration artifacts: %s\n' "$SMOOTH_SCROLL_TEST_DIR"
exit "$status"
