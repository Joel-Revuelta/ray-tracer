#!/usr/bin/env bash
set -euo pipefail

if [ "$#" -ne 3 ]; then
    echo "usage: $0 <raytracer> <source-dir> <build-dir>" >&2
    exit 2
fi

executable="$1"
source_dir="$2"
build_dir="$3"
out_dir="${build_dir}/parity"

if ! "${executable}" --list-renderers 2>/dev/null | grep -q 'cuda.*\[available\]'; then
    echo "CUDA renderer unavailable; skipping CPU/CUDA parity test"
    exit 77
fi

rm -rf "${out_dir}"
mkdir -p "${out_dir}"

render() {
    local renderer="$1"
    local name="$2"

    shift 2
    if ! "${executable}" --path "${source_dir}/scenes/brass_orrery.cfg" --cli \
        --renderer "${renderer}" --width 128 --height 72 --samples 64 --seed 5 \
        "$@" --output "${out_dir}/${name}.ppm" >"${out_dir}/${name}.log" 2>&1; then
        cat "${out_dir}/${name}.log" >&2
        return 1
    fi
}

render cpu still-cpu
render cuda still-cuda
render cpu anim-cpu --animation --start-frame 3 --end-frame 3
render cuda anim-cuda --animation --start-frame 3 --end-frame 3
render cpu blur-cpu --animation --start-frame 3 --end-frame 3 --shutter-angle 180 --shutter-samples 4
render cuda blur-cuda --animation --start-frame 3 --end-frame 3 --shutter-angle 180 --shutter-samples 4

compare() {
    local first="$1"
    local second="$2"
    local label="$3"

    python3 - "${first}" "${second}" "${label}" <<'PY'
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
        raise SystemExit(f"{path}: expected 8-bit PPM, got max value {maximum}")

    index += 1
    payload = data[index:index + width * height * 3]

    if len(payload) != width * height * 3:
        raise SystemExit(f"{path}: truncated pixel payload")

    return width, height, payload


first_width, first_height, first = read_ppm(sys.argv[1])
second_width, second_height, second = read_ppm(sys.argv[2])
label = sys.argv[3]

if (first_width, first_height) != (second_width, second_height):
    raise SystemExit(f"{label}: image dimensions differ")

total = 0
maximum = 0
per_pixel = []

for offset in range(0, len(first), 3):
    differences = [abs(first[offset + channel] - second[offset + channel]) for channel in range(3)]
    total += sum(differences)
    pixel_maximum = max(differences)
    maximum = max(maximum, pixel_maximum)
    per_pixel.append(pixel_maximum)

per_pixel.sort()
count = len(per_pixel)
mean = total / (count * 3.0)
p99 = per_pixel[int(0.99 * (count - 1))]
identical = sum(1 for value in per_pixel if value == 0) * 100.0 / count

print(f"{label}: mean={mean:.4f}/255 p99={p99} max={maximum} identical={identical:.2f}%")

if mean > 0.5 or p99 > 8 or maximum > 64:
    raise SystemExit(f"{label}: parity thresholds exceeded")
PY
}

compare "${out_dir}/still-cpu.ppm" "${out_dir}/still-cuda.ppm" "still"
compare "${out_dir}/anim-cpu-0003.ppm" "${out_dir}/anim-cuda-0003.ppm" "animation frame 3"
compare "${out_dir}/blur-cpu-0003.ppm" "${out_dir}/blur-cuda-0003.ppm" "motion-blurred frame 3"

# CUDA resume must initialize the session from the first missing frame and
# leave existing frames untouched.
resume_dir="${out_dir}/resume"

mkdir -p "${resume_dir}"
if ! "${executable}" --path "${source_dir}/scenes/brass_orrery.cfg" --cli --renderer cuda \
    --width 32 --height 18 --samples 1 --animation --start-frame 12 --end-frame 13 \
    --output "${resume_dir}/f-%02d.ppm" >"${out_dir}/resume.log" 2>&1; then
    cat "${out_dir}/resume.log" >&2
    exit 1
fi

before="$(md5sum "${resume_dir}/f-12.ppm" | cut -d' ' -f1)"
rm "${resume_dir}/f-13.ppm"

if ! "${executable}" --path "${source_dir}/scenes/brass_orrery.cfg" --cli --renderer cuda \
    --width 32 --height 18 --samples 1 --animation --start-frame 12 --end-frame 13 --resume \
    --output "${resume_dir}/f-%02d.ppm" >"${out_dir}/resume.log" 2>&1; then
    cat "${out_dir}/resume.log" >&2
    exit 1
fi

after="$(md5sum "${resume_dir}/f-12.ppm" | cut -d' ' -f1)"

if [ "${before}" != "${after}" ] || [ ! -s "${resume_dir}/f-13.ppm" ]; then
    echo "CUDA resume did not preserve and complete the sequence" >&2
    exit 1
fi
