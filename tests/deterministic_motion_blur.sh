#!/usr/bin/env bash
set -euo pipefail

if [ "$#" -ne 3 ]; then
    echo "usage: $0 <raytracer> <source-dir> <build-dir>" >&2
    exit 2
fi

executable="$1"
source_dir="$2"
build_dir="$3"
out_dir="${build_dir}/motion-blur"

rm -rf "${out_dir}"
mkdir -p "${out_dir}"

render() {
    local directory="$1"

    mkdir -p "${directory}"
    "${executable}" --path "${source_dir}/scenes/brass_orrery.cfg" --cli --renderer cpu \
        --width 64 --height 36 --samples 2 --seed 7 \
        --animation --start-frame 0 --end-frame 1 \
        --shutter-angle 180 --shutter-samples 4 \
        --output "${directory}/frame-%02d.ppm" >/dev/null
}

render "${out_dir}/first"
render "${out_dir}/second"

for frame in 00 01; do
    cmp "${out_dir}/first/frame-${frame}.ppm" "${out_dir}/second/frame-${frame}.ppm"
done

# Shutter-off and motion-blurred renders must differ.
"${executable}" --path "${source_dir}/scenes/brass_orrery.cfg" --cli --renderer cpu \
    --width 64 --height 36 --samples 2 --seed 7 \
    --animation --start-frame 0 --end-frame 0 \
    --output "${out_dir}/sharp-%02d.ppm" >/dev/null

"${executable}" --path "${source_dir}/scenes/brass_orrery.cfg" --cli --renderer cpu \
    --width 64 --height 36 --samples 2 --seed 7 \
    --animation --start-frame 0 --end-frame 0 \
    --shutter-angle 180 --shutter-samples 4 \
    --output "${out_dir}/blurred-%02d.ppm" >/dev/null

if cmp -s "${out_dir}/sharp-00.ppm" "${out_dir}/blurred-00.ppm"; then
    echo "motion blur did not change the rendered frame" >&2
    exit 1
fi

# Scene-authored shutter settings must be parsed and used.
"${executable}" --path "${source_dir}/tests/animation_shutter.cfg" --cli --renderer cpu \
    --width 64 --height 36 --samples 2 --seed 7 \
    --animation --start-frame 12 --end-frame 12 \
    --output "${out_dir}/scene-%02d.ppm" >/dev/null

python3 - "${out_dir}/scene.json" <<'PY'
import json
import sys

with open(sys.argv[1], encoding="utf-8") as handle:
    data = json.load(handle)

shutter = data.get("shutter", {})
assert shutter.get("angle") == 180.0, shutter
assert shutter.get("samples") == 3, shutter
assert data.get("fps") == 24, data.get("fps")
assert len(data.get("frames", [])) == 1, data.get("frames")

frame = data["frames"][0]
assert frame["frame"] == 12, frame
assert frame["rendered"] is True, frame
assert frame["camera"]["fieldOfView"] > 0.0, frame
PY

# Resume skips existing frames and rewrites metadata for the whole range.
resume_dir="${out_dir}/resume"

mkdir -p "${resume_dir}"
"${executable}" --path "${source_dir}/scenes/brass_orrery.cfg" --cli --renderer cpu \
    --width 64 --height 36 --samples 2 --seed 7 \
    --animation --start-frame 0 --end-frame 1 \
    --output "${resume_dir}/f-%02d.ppm" >/dev/null

before="$(md5sum "${resume_dir}/f-00.ppm" | cut -d' ' -f1)"
rm "${resume_dir}/f-01.ppm"

"${executable}" --path "${source_dir}/scenes/brass_orrery.cfg" --cli --renderer cpu \
    --width 64 --height 36 --samples 2 --seed 7 \
    --animation --start-frame 0 --end-frame 1 --resume \
    --output "${resume_dir}/f-%02d.ppm" >/dev/null

after="$(md5sum "${resume_dir}/f-00.ppm" | cut -d' ' -f1)"

if [ "${before}" != "${after}" ] || [ ! -s "${resume_dir}/f-01.ppm" ]; then
    echo "resume did not preserve and complete the sequence" >&2
    exit 1
fi

python3 - "${resume_dir}/f.json" <<'PY'
import json
import sys

with open(sys.argv[1], encoding="utf-8") as handle:
    data = json.load(handle)

assert len(data["frames"]) == 2, data["frames"]
assert all(frame["rendered"] for frame in data["frames"]), data["frames"]
PY

if find "${out_dir}" -name '*.tmp' | grep -q .; then
    echo "temporary files were left behind" >&2
    exit 1
fi
