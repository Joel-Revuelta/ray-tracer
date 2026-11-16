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
output="$(mktemp "${TMPDIR:-/tmp}/raytracer-gui-output.XXXXXX.ppm")"
reference="$(mktemp "${TMPDIR:-/tmp}/raytracer-gui-reference.XXXXXX.ppm")"
trap 'rm -f "$log" "$output" "$reference"' EXIT

set +e
timeout 5 "$executable" --path "$scene" --gui "$backend" --width 64 --height 36 --frames 1 \
    --output "$output" >"$log" 2>&1
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

# The renderer writes --output before entering the GUI event loop. Validate the
# resulting binary PPM so the test covers the full display/output path.
python3 - "$output" <<'PY'
import sys

path = sys.argv[1]

with open(path, "rb") as handle:
    data = handle.read()

parts = data.split(maxsplit=4)

if len(parts) != 5 or parts[0] != b"P6" or parts[1] != b"64" or parts[2] != b"36" or parts[3] != b"255":
    raise SystemExit(f"GUI output has an invalid PPM header: {path}")
if len(parts[4]) != 64 * 36 * 3:
    raise SystemExit(f"GUI output has an invalid PPM payload: {path}")
PY

# The GUI path must produce exactly the same image as the deterministic CLI
# path for the same scene, resolution, sample count and seed.
"$executable" --path "$scene" --cli --renderer cpu --width 64 --height 36 \
    --samples 1 --seed 0 --output "$reference" >/dev/null 2>&1
if ! cmp -s "$output" "$reference"; then
    echo "GUI backend $backend output differs from the deterministic CLI render" >&2
    exit 1
fi

exit 0
