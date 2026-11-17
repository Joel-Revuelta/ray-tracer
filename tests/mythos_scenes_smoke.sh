#!/usr/bin/env bash
set -euo pipefail

if [ "$#" -ne 3 ]; then
    echo "usage: $0 <raytracer> <source-dir> <build-dir>" >&2
    exit 2
fi

executable="$1"
source_dir="$2"
build_dir="$3"
out_dir="${build_dir}/mythos-smoke"

rm -rf "$out_dir"
mkdir -p "$out_dir"

scenes=(
    impossible_glass_garden
    last_leviathan
    cathedral_machine_sun
    serpent_of_stars
    clockwork_aurora
)

for scene in "${scenes[@]}"; do
    "$executable" --path "${source_dir}/scenes/${scene}.cfg" --cli --renderer cpu \
        --width 32 --height 18 --samples 1 --seed 5 \
        --output "${out_dir}/${scene}.ppm" >"${out_dir}/${scene}.log" 2>&1

    if [ ! -s "${out_dir}/${scene}.ppm" ]; then
        echo "${scene}: smoke render did not produce an image" >&2
        exit 1
    fi
done

for scene in impossible_glass_garden serpent_of_stars clockwork_aurora; do
    "$executable" --path "${source_dir}/scenes/${scene}.cfg" --cli --renderer cpu \
        --width 32 --height 18 --samples 1 --seed 5 --animation \
        --start-frame 0 --end-frame 0 \
        --output "${out_dir}/${scene}-frame-%04d.ppm" >"${out_dir}/${scene}-animation.log" 2>&1

    if [ ! -s "${out_dir}/${scene}-frame-0000.ppm" ]; then
        echo "${scene}: animated smoke render did not produce frame 0" >&2
        exit 1
    fi
done
