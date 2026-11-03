#!/usr/bin/env bash
set -euo pipefail

cd "$(dirname "$0")/.."

FRAMES="${FRAMES:-140}"
WIDTH="${WIDTH:-1920}"
HEIGHT="${HEIGHT:-1080}"
OUT_DIR="${OUT_DIR:-screenshots/hd}"

SCENES=("$@")
if [ ${#SCENES[@]} -eq 0 ]; then
    SCENES=(neon_district crystal_garden chrome_singularity solar_bloom)
fi

mkdir -p "$OUT_DIR"

for scene in "${SCENES[@]}"; do
    echo "Rendering ${scene} at ${WIDTH}x${HEIGHT} with ${FRAMES} frames..."
    ./raytracer --path "scenes/${scene}.cfg" --cli --width "$WIDTH" --height "$HEIGHT" --frames "$FRAMES" \
        --output "${OUT_DIR}/${scene}.ppm"
done

echo "Done. Images are in ${OUT_DIR}/"
