#!/usr/bin/env bash
set -euo pipefail

if [ "$#" -ne 3 ]; then
    echo "usage: $0 <raytracer> <source-dir> <build-dir>" >&2
    exit 2
fi

executable="$1"
source_dir="$2"
build_dir="$3"
out_dir="${build_dir}/motion-blur-bvh"

rm -rf "$out_dir"
mkdir -p "$out_dir"

render() {
    local mode="$1"
    local output="$2"

    if [ "$mode" = "linear" ]; then
        RAYTRACER_DISABLE_BVH=1 "${executable}" --path "${source_dir}/scenes/motion_blur.cfg" --cli \
            --renderer cpu --width 64 --height 36 --samples 8 --seed 5 \
            --animation --start-frame 24 --end-frame 24 \
            --output "$output" >"${out_dir}/${mode}.log" 2>&1
    else
        "${executable}" --path "${source_dir}/scenes/motion_blur.cfg" --cli \
            --renderer cpu --width 64 --height 36 --samples 8 --seed 5 \
            --animation --start-frame 24 --end-frame 24 \
            --output "$output" >"${out_dir}/${mode}.log" 2>&1
    fi
}

render bvh "${out_dir}/bvh-%02d.ppm"
render linear "${out_dir}/linear-%02d.ppm"

if ! cmp -s "${out_dir}/bvh-24.ppm" "${out_dir}/linear-24.ppm"; then
    echo "refit BVH diverged from the linear scan for an animated frame" >&2
    exit 1
fi
