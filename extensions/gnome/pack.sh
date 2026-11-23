#!/bin/sh
set -eu
cd "$(dirname "$0")"
output_dir=${1:-/tmp/smooth-scroll-extension}
mkdir -p "$output_dir"
output_dir=$(cd "$output_dir" && pwd)
glib-compile-schemas --strict --dry-run smooth-scroll@wayne6530/schemas
gnome-extensions pack --force --out-dir="$output_dir" \
    --extra-source=settings.js --extra-source=rules.js \
    --extra-source=settingsStore.js --extra-source=preferences.js \
    --extra-source=bridgeProtocol.js --extra-source=shellBridge.js \
    smooth-scroll@wayne6530
