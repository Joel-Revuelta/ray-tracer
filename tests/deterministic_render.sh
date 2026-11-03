#!/usr/bin/env bash
set -euo pipefail

if [ "$#" -lt 3 ] || [ "$#" -gt 4 ]; then
    echo "usage: $0 <raytracer> <source-dir> <build-dir> [seed]" >&2
    exit 2
fi

executable="$1"
source_dir="$2"
build_dir="$3"
seed="${4:-0}"

first="${build_dir}/deterministic-1.ppm"
second="${build_dir}/deterministic-2.ppm"

"${executable}" --path "${source_dir}/scenes/showcase.cfg" --cli --renderer cpu \
    --width 32 --height 18 --frames 4 --seed "${seed}" --output "${first}" >/dev/null
"${executable}" --path "${source_dir}/scenes/showcase.cfg" --cli --renderer cpu \
    --width 32 --height 18 --frames 4 --seed "${seed}" --output "${second}" >/dev/null

cmp "${first}" "${second}"
