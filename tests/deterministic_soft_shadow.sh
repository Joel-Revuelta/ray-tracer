#!/usr/bin/env bash
set -euo pipefail

if [ "$#" -ne 3 ]; then
    echo "usage: $0 <raytracer> <source-dir> <build-dir>" >&2
    exit 2
fi

executable="$1"
source_dir="$2"
build_dir="$3"
first="${build_dir}/soft-shadow-deterministic-1.ppm"
second="${build_dir}/soft-shadow-deterministic-2.ppm"

"${executable}" --path "${source_dir}/scenes/soft_shadows.cfg" --cli --renderer cpu \
    --width 64 --height 36 --frames 4 --seed 19 --output "${first}" >/dev/null
"${executable}" --path "${source_dir}/scenes/soft_shadows.cfg" --cli --renderer cpu \
    --width 64 --height 36 --frames 4 --seed 19 --output "${second}" >/dev/null

cmp "${first}" "${second}"
