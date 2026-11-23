#!/bin/sh
set -eu
cd "$(dirname "$0")/.."
schema_dir=$(mktemp -d)
trap 'rm -rf "$schema_dir"' EXIT HUP INT TERM
glib-compile-schemas --strict --targetdir="$schema_dir" smooth-scroll@wayne6530/schemas
for test in tests/*.test.cjs; do
    node "$test"
done
GSETTINGS_SCHEMA_DIR="$schema_dir" GSETTINGS_BACKEND=memory gjs -m tests/settings.test.js
for module in smooth-scroll@wayne6530/*.js; do
    node --input-type=module --check < "$module"
done
