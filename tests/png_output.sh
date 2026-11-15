#!/usr/bin/env bash
set -euo pipefail

if [ "$#" -ne 3 ]; then
    echo "usage: $0 <raytracer> <source-dir> <build-dir>" >&2
    exit 2
fi

executable="$1"
source_dir="$2"
build_dir="$3"

still="${build_dir}/png-still-smoke.png"
animated="${build_dir}/png-animation-smoke-%02d.png"

"${executable}" --path "${source_dir}/scenes/showcase.cfg" --cli --renderer cpu \
    --width 32 --height 18 --samples 1 --seed 3 --output "${still}" >/dev/null

"${executable}" --path "${source_dir}/scenes/brass_orrery.cfg" --cli --renderer cpu \
    --width 32 --height 18 --samples 1 --seed 3 \
    --animation --start-frame 0 --end-frame 1 \
    --output "${animated}" >/dev/null

check_png() {
    local path="$1"
    local signature

    if [ ! -s "${path}" ]; then
        echo "missing PNG output: ${path}" >&2
        exit 1
    fi

    signature="$(od -An -tx1 -N8 "${path}" | tr -d ' \n')"

    if [ "${signature}" != "89504e470d0a1a0a" ]; then
        echo "not a PNG file: ${path} (signature: ${signature})" >&2
        exit 1
    fi
}

check_png "${still}"
check_png "${build_dir}/png-animation-smoke-00.png"
check_png "${build_dir}/png-animation-smoke-01.png"
