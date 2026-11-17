#!/usr/bin/env bash
set -euo pipefail

# Render the three MYTHOS films at 1080p. Run from the repository root.
executable="${RAYTRACER:-./raytracer}"
renderer="${RENDERER:-cuda}"
samples="${SAMPLES:-32}"
width="${WIDTH:-1920}"
height="${HEIGHT:-1080}"
output_dir="${OUTPUT_DIR:-screenshots/mythos}"

mkdir -p "$output_dir"

render() {
    local scene="$1"
    local name="$2"
    local exposure="$3"
    local threshold="$4"
    local intensity="$5"
    local lens="$6"

    echo "Rendering ${name} (${width}x${height}, ${samples} samples)..." >&2
    "$executable" --path "scenes/${scene}.cfg" --cli --renderer "$renderer" \
        --width "$width" --height "$height" --samples "$samples" --seed 7 \
        --tonemap aces --exposure "$exposure" \
        --bloom --bloom-threshold "$threshold" --bloom-intensity "$intensity" \
        --lens-radius "$lens" --animation --start-frame 0 --end-frame 191 \
        --shutter-angle 180 --shutter-samples 4 \
        --video "${output_dir}/${name}.mp4"
}

render impossible_glass_garden impossible_glass_garden 0.95 2.8 0.25 0.04
render serpent_of_stars serpent_of_stars 0.95 2.2 0.30 0.02
render clockwork_aurora clockwork_aurora 1.00 2.2 0.30 0.02
