#!/usr/bin/env bash
set -euo pipefail

cd "$(dirname "$0")/.."

WIDTH="${WIDTH:-7680}"
HEIGHT="${HEIGHT:-4320}"
FRAMES="${FRAMES:-2048}"
RENDERER="${RENDERER:-cuda}"
OUT_DIR="${OUT_DIR:-screenshots/8k}"

SCENES=("$@")
if [ ${#SCENES[@]} -eq 0 ]; then
    SCENES=(hypernova_cathedral infinity_mirror crystal_superbloom solar_eclipse)
fi

mkdir -p "$OUT_DIR"

for scene in "${SCENES[@]}"; do
    echo "Rendering ${scene} at ${WIDTH}x${HEIGHT} with ${FRAMES} frames using ${RENDERER}..."
    start=$(date +%s)
    ./raytracer --path "scenes/${scene}.cfg" --cli --renderer "$RENDERER" \
        --width "$WIDTH" --height "$HEIGHT" --frames "$FRAMES" \
        --output "${OUT_DIR}/${scene}.ppm"
    end=$(date +%s)
    echo "  ${scene} took $((end - start))s"
done

if [ "${CONVERT:-1}" = "0" ]; then
    echo "Done. PPM images are in ${OUT_DIR}/"
    exit 0
fi

python3 - "$OUT_DIR" "${SCENES[@]}" <<'PY'
import os
import sys
from PIL import Image

out_dir = sys.argv[1]
names = sys.argv[2:]
previews = []

for name in names:
    ppm = os.path.join(out_dir, name + ".ppm")
    if not os.path.exists(ppm):
        print(f"missing {ppm}, skipping")
        continue

    print(f"Converting {name}...")
    image = Image.open(ppm).convert("RGB")
    image.save(os.path.join(out_dir, name + ".png"))

    for suffix, size in (("4k", (3840, 2160)), ("1080p", (1920, 1080))):
        preview = image.resize(size, Image.Resampling.LANCZOS)
        preview.save(os.path.join(out_dir, f"{name}-{suffix}.png"))
        if suffix == "1080p":
            previews.append((name, preview))

if previews:
    sheet = Image.new("RGB", (1920 * 2, 1080 * 2), (15, 15, 15))
    for index, (name, preview) in enumerate(previews):
        sheet.paste(preview, ((index % 2) * 1920, (index // 2) * 1080))
    sheet.save(os.path.join(out_dir, "contact.png"))
    print(f"contact sheet written to {os.path.join(out_dir, 'contact.png')}")
PY

echo "Done. Images are in ${OUT_DIR}/"
