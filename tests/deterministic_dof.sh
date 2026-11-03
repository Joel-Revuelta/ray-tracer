#!/usr/bin/env bash
set -euo pipefail

if [ "$#" -ne 3 ]; then
    echo "usage: $0 <raytracer> <source-dir> <build-dir>" >&2
    exit 2
fi

executable="$1"
source_dir="$2"
build_dir="$3"
first="${build_dir}/dof-deterministic-1.ppm"
second="${build_dir}/dof-deterministic-2.ppm"

"${executable}" --path "${source_dir}/scenes/depth_of_field.cfg" --cli --renderer cpu \
    --width 64 --height 36 --frames 4 --seed 17 --output "${first}" >/dev/null
"${executable}" --path "${source_dir}/scenes/depth_of_field.cfg" --cli --renderer cpu \
    --width 64 --height 36 --frames 4 --seed 17 --output "${second}" >/dev/null

cmp "${first}" "${second}"
