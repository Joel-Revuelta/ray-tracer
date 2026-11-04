#!/usr/bin/env bash
set -euo pipefail

if [ "$#" -ne 3 ]; then
    echo "usage: $0 <raytracer> <source-dir> <build-dir>" >&2
    exit 2
fi

executable="$1"
source_dir="$2"
build_dir="$3"
scenes=(sakura_shrine brass_orrery tidal_bloom obsidian_aurora frozen_cathedral)

for scene in "${scenes[@]}"; do
    first="${build_dir}/${scene}-deterministic-1.ppm"
    second="${build_dir}/${scene}-deterministic-2.ppm"

    "${executable}" --path "${source_dir}/scenes/${scene}.cfg" --cli --renderer cpu \
        --width 64 --height 36 --frames 4 --seed 43 --output "${first}" >/dev/null
    "${executable}" --path "${source_dir}/scenes/${scene}.cfg" --cli --renderer cpu \
        --width 64 --height 36 --frames 4 --seed 43 --output "${second}" >/dev/null

    cmp "${first}" "${second}"
done
