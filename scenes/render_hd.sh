#!/usr/bin/env bash
set -euo pipefail

cd "$(dirname "$0")/.."

RAYTRACER_BIN="${RAYTRACER_BIN:-./raytracer}"
if [ ! -x "$RAYTRACER_BIN" ] && [ -x "./build/raytracer" ]; then
    RAYTRACER_BIN="./build/raytracer"
fi

FRAMES="${FRAMES:-140}"
WIDTH="${WIDTH:-1920}"
HEIGHT="${HEIGHT:-1080}"
RENDERER="${RENDERER:-cpu}"
if [ -z "${OUT_DIR:-}" ]; then
    if [ "$RENDERER" = "cpu" ]; then
        OUT_DIR="screenshots/hd"
    else
        OUT_DIR="screenshots/hd-${RENDERER}"
    fi
fi

SCENES=("$@")
if [ ${#SCENES[@]} -eq 0 ]; then
    SCENES=(neon_district crystal_garden chrome_singularity solar_bloom)
fi

mkdir -p "$OUT_DIR"

for scene in "${SCENES[@]}"; do
    echo "Rendering ${scene} at ${WIDTH}x${HEIGHT} with ${FRAMES} frames using the ${RENDERER} renderer..."
    "$RAYTRACER_BIN" --path "scenes/${scene}.cfg" --cli --renderer "$RENDERER" --width "$WIDTH" --height "$HEIGHT" \
        --frames "$FRAMES" --output "${OUT_DIR}/${scene}.ppm"
done

echo "Done. Images are in ${OUT_DIR}/"
