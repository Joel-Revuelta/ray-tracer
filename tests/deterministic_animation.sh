#!/usr/bin/env bash
set -euo pipefail

if [ "$#" -ne 3 ]; then
    echo "usage: $0 <raytracer> <source-dir> <build-dir>" >&2
    exit 2
fi

executable="$1"
source_dir="$2"
build_dir="$3"

first_dir="${build_dir}/animation-deterministic-1"
second_dir="${build_dir}/animation-deterministic-2"

rm -rf "${first_dir}" "${second_dir}"
mkdir -p "${first_dir}" "${second_dir}"

render() {
    local output_dir="$1"

    "${executable}" --path "${source_dir}/scenes/brass_orrery.cfg" --cli --renderer cpu \
        --width 48 --height 27 --samples 2 --seed 11 \
        --animation --start-frame 0 --end-frame 2 \
        --output "${output_dir}/frame-%04d.ppm" >/dev/null
}

render "${first_dir}"
render "${second_dir}"

for frame in 0000 0001 0002; do
    cmp "${first_dir}/frame-${frame}.ppm" "${second_dir}/frame-${frame}.ppm"
done

# PNG output must be a real PNG and must follow the numbered pattern.
"${executable}" --path "${source_dir}/scenes/brass_orrery.cfg" --cli --renderer cpu \
    --width 32 --height 18 --samples 1 --seed 11 \
    --animation --start-frame 1 --end-frame 1 \
    --output "${build_dir}/animation-frame-%02d.png" >/dev/null

png="${build_dir}/animation-frame-01.png"

if [ ! -s "${png}" ]; then
    echo "animation PNG frame was not written" >&2
    exit 1
fi

signature="$(od -An -tx1 -N8 "${png}" | tr -d ' \n')"

if [ "${signature}" != "89504e470d0a1a0a" ]; then
    echo "animation output is not a PNG (signature: ${signature})" >&2
    exit 1
fi
