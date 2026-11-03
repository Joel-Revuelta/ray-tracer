#!/usr/bin/env bash
set -euo pipefail

if [ "$#" -ne 3 ]; then
    echo "usage: $0 <raytracer> <source-dir> <build-dir>" >&2
    exit 2
fi

executable="$1"
source_dir="$2"
build_dir="$3"
scene="${source_dir}/scenes/meshes.cfg"
linear="${build_dir}/mesh_parity_linear.ppm"
accelerated="${build_dir}/mesh_parity_bvh.ppm"

RAYTRACER_DISABLE_BVH=1 "${executable}" --path "${scene}" --cli --renderer cpu \
    --width 64 --height 36 --frames 2 --seed 11 --output "${linear}" >/dev/null

"${executable}" --path "${scene}" --cli --renderer cpu \
    --width 64 --height 36 --frames 2 --seed 11 --output "${accelerated}" >/dev/null

cmp "${linear}" "${accelerated}"
