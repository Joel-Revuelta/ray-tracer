#!/usr/bin/env bash
set -euo pipefail

if [ "$#" -ne 3 ]; then
    echo "usage: $0 <raytracer> <source-dir> <build-dir>" >&2
    exit 2
fi

executable="$1"
source_dir="$2"
build_dir="$3"
out_dir="${build_dir}/golden-images"
golden_dir="${source_dir}/tests/golden"
update="${UPDATE_GOLDEN:-0}"

mkdir -p "$out_dir"
if [ "$update" = "1" ]; then
    mkdir -p "$golden_dir"
fi

render() {
    local name="$1"
    local scene="$2"
    local frame="$3"
    local samples=8

    shift 3
    if [ "$#" -gt 0 ] && [[ "$1" =~ ^[0-9]+$ ]]; then
        samples="$1"
        shift
    fi
    local output

    if [ "$frame" = "still" ]; then
        output="$out_dir/${name}.ppm"
    else
        output="$out_dir/${name}-%04d.ppm"
    fi

    if ! "$executable" --path "$source_dir/scenes/${scene}.cfg" --cli --renderer cpu \
        --width 96 --height 54 --samples "$samples" --seed 5 \
        "$@" --output "$output" >"$out_dir/${name}.log" 2>&1; then
        cat "$out_dir/${name}.log" >&2
        return 1
    fi
}

compare() {
    local name="$1"
    local actual="$2"
    local reference="$golden_dir/${name}.ppm"

    if [ "$update" = "1" ]; then
        cp "$actual" "$reference"
        echo "Updated golden reference: ${name}"
        return 0
    fi
    if [ ! -f "$reference" ]; then
        echo "Missing golden reference ${reference}; run UPDATE_GOLDEN=1 $0 ..." >&2
        return 1
    fi

    python3 - "$reference" "$actual" "$name" <<'PY'
import sys


def read_ppm(path):
    with open(path, "rb") as handle:
        data = handle.read()

    if not data.startswith(b"P6"):
        raise SystemExit(f"{path}: not a binary PPM")

    index = 2
    tokens = []

    while len(tokens) < 3:
        while index < len(data) and data[index] in b" \t\r\n":
            index += 1
        if index < len(data) and data[index:index + 1] == b"#":
            while index < len(data) and data[index] not in b"\r\n":
                index += 1
            continue

        start = index
        while index < len(data) and data[index] not in b" \t\r\n":
            index += 1
        tokens.append(data[start:index])

    width = int(tokens[0])
    height = int(tokens[1])
    maximum = int(tokens[2])

    if maximum != 255:
        raise SystemExit(f"{path}: expected 8-bit PPM")

    index += 1
    pixels = data[index:index + width * height * 3]

    if len(pixels) != width * height * 3:
        raise SystemExit(f"{path}: truncated pixel data")

    return width, height, pixels


reference_width, reference_height, reference = read_ppm(sys.argv[1])
actual_width, actual_height, actual = read_ppm(sys.argv[2])
name = sys.argv[3]

if (reference_width, reference_height) != (actual_width, actual_height):
    raise SystemExit(f"{name}: image dimensions differ")

total = 0
maximum = 0
per_pixel = []

for offset in range(0, len(reference), 3):
    differences = [abs(reference[offset + channel] - actual[offset + channel]) for channel in range(3)]
    total += sum(differences)
    pixel_maximum = max(differences)
    maximum = max(maximum, pixel_maximum)
    per_pixel.append(pixel_maximum)

per_pixel.sort()
count = len(per_pixel)
mean = total / (count * 3.0)
p99 = per_pixel[int(0.99 * (count - 1))]
print(f"{name}: mean={mean:.4f}/255 p99={p99} max={maximum}")

# Mean and p99 catch structural regressions. A single HDR firefly can still
# shift one channel on a different libm/CPU implementation, so allow a larger
# isolated per-pixel outlier than the broad-difference limits.
if mean > 1.0 or p99 > 8 or maximum > 160:
    raise SystemExit(f"{name}: golden image thresholds exceeded")
PY
}

render showcase-still showcase still
render glass_menagerie-still glass_menagerie still
render brass_orrery-still brass_orrery still
render material_maps-still material_maps still
render mtl_maps-still mtl_maps still
render misty_shrine-still misty_shrine still 256
render volumetric_cathedral-still volumetric_cathedral still 256
render lagoon_mist-still lagoon_mist still 256
render motion_blur-animation motion_blur 24 64 --animation --start-frame 24 --end-frame 24
render primitives_extra-still primitives_extra still
render scatter_field-still scatter_field still
render brass_orrery-animation brass_orrery 96 --animation --start-frame 96 --end-frame 96
render sakura_shrine-animation sakura_shrine 96 --animation --start-frame 96 --end-frame 96
render tidal_bloom-animation tidal_bloom 96 --tonemap aces --exposure 1.15 \
    --bloom --bloom-threshold 2.5 --bloom-intensity 0.2 \
    --animation --start-frame 96 --end-frame 96
render brass_orrery-motion-blur brass_orrery 3 --animation --start-frame 3 --end-frame 3 \
    --shutter-angle 180 --shutter-samples 4

compare showcase-still "$out_dir/showcase-still.ppm"
compare glass_menagerie-still "$out_dir/glass_menagerie-still.ppm"
compare brass_orrery-still "$out_dir/brass_orrery-still.ppm"
compare material_maps-still "$out_dir/material_maps-still.ppm"
compare mtl_maps-still "$out_dir/mtl_maps-still.ppm"
compare misty_shrine-still "$out_dir/misty_shrine-still.ppm"
compare volumetric_cathedral-still "$out_dir/volumetric_cathedral-still.ppm"
compare lagoon_mist-still "$out_dir/lagoon_mist-still.ppm"
compare motion_blur-animation "$out_dir/motion_blur-animation-0024.ppm"
compare primitives_extra-still "$out_dir/primitives_extra-still.ppm"
compare scatter_field-still "$out_dir/scatter_field-still.ppm"
compare brass_orrery-animation "$out_dir/brass_orrery-animation-0096.ppm"
compare sakura_shrine-animation "$out_dir/sakura_shrine-animation-0096.ppm"
compare tidal_bloom-animation "$out_dir/tidal_bloom-animation-0096.ppm"
compare brass_orrery-motion-blur "$out_dir/brass_orrery-motion-blur-0003.ppm"
