#!/usr/bin/env bash
set -euo pipefail

if [ "$#" -ne 3 ]; then
    echo "usage: $0 <raytracer> <source-dir> <build-dir>" >&2
    exit 2
fi

executable="$1"
source_dir="$2"
build_dir="$3"

first="${build_dir}/deterministic-1.ppm"
second="${build_dir}/deterministic-2.ppm"

"${executable}" --path "${source_dir}/scenes/showcase.cfg" --cli --renderer cpu \
    --width 32 --height 18 --frames 4 --output "${first}" >/dev/null
"${executable}" --path "${source_dir}/scenes/showcase.cfg" --cli --renderer cpu \
    --width 32 --height 18 --frames 4 --output "${second}" >/dev/null

cmp "${first}" "${second}"
