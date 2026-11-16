#!/usr/bin/env bash
set -euo pipefail

if [ "$#" -ne 3 ]; then
    echo "usage: $0 <raytracer> <scene> <sfml|opengl>" >&2
    exit 2
fi

executable="$1"
scene="$2"
backend="$3"
log="$(mktemp "${TMPDIR:-/tmp}/raytracer-gui-smoke.XXXXXX")"
trap 'rm -f "$log"' EXIT

set +e
timeout 5 "$executable" --path "$scene" --gui "$backend" --width 64 --height 36 --frames 1 \
    >"$log" 2>&1
status=$?
set -e

# The GUI is event-driven and intentionally stays open after rendering one
# frame, so a clean timeout is the expected outcome of this smoke test.
if [ "$status" -ne 124 ]; then
    echo "GUI backend $backend exited with status $status" >&2
    cat "$log" >&2
    exit 1
fi

# A successful run should have produced the progress/rendering output and must
# not report a backend load failure.
if ! grep -q "Rendering image" "$log"; then
    echo "GUI backend $backend did not start rendering" >&2
    cat "$log" >&2
    exit 1
fi

exit 0
