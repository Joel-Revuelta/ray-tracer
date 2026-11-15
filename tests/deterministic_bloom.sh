#!/usr/bin/env bash
set -euo pipefail

if [ "$#" -ne 3 ]; then
    echo "usage: $0 <raytracer> <source-dir> <build-dir>" >&2
    exit 2
fi

executable="$1"
source_dir="$2"
build_dir="$3"
first="${build_dir}/bloom-deterministic-1.ppm"
second="${build_dir}/bloom-deterministic-2.ppm"

"${executable}" --path "${source_dir}/scenes/tidal_bloom.cfg" --cli --renderer cpu \
    --width 64 --height 36 --frames 4 --seed 53 \
    --tonemap aces --exposure 1.15 --bloom --bloom-threshold 1.0 \
    --bloom-intensity 0.45 --bloom-radius 8 --output "${first}" >/dev/null
"${executable}" --path "${source_dir}/scenes/tidal_bloom.cfg" --cli --renderer cpu \
    --width 64 --height 36 --frames 4 --seed 53 \
    --tonemap aces --exposure 1.15 --bloom --bloom-threshold 1.0 \
    --bloom-intensity 0.45 --bloom-radius 8 --output "${second}" >/dev/null

cmp "${first}" "${second}"
