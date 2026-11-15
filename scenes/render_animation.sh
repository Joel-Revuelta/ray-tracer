#!/usr/bin/env bash
set -euo pipefail

cd "$(dirname "$0")/.."

SCENE="${SCENE:-${1:-brass_orrery}}"
SAMPLES="${SAMPLES:-128}"
WIDTH="${WIDTH:-1280}"
HEIGHT="${HEIGHT:-720}"
RENDERER="${RENDERER:-cpu}"
FPS="${FPS:-}"
START_FRAME="${START_FRAME:-}"
END_FRAME="${END_FRAME:-}"
FFMPEG="${FFMPEG:-0}"
OUT_DIR="${OUT_DIR:-animations/${SCENE}}"
PATTERN="${PATTERN:-${OUT_DIR}/frame-%04d.png}"

TONE_FLAGS=()
if [ -n "${TONEMAP:-}" ]; then
    TONE_FLAGS+=(--tonemap "$TONEMAP")
fi
if [ -n "${EXPOSURE:-}" ]; then
    TONE_FLAGS+=(--exposure "$EXPOSURE")
fi
if [ "${BLOOM:-0}" = "1" ]; then
    TONE_FLAGS+=(--bloom)
fi
if [ -n "${BLOOM_THRESHOLD:-}" ]; then
    TONE_FLAGS+=(--bloom-threshold "$BLOOM_THRESHOLD")
fi
if [ -n "${BLOOM_INTENSITY:-}" ]; then
    TONE_FLAGS+=(--bloom-intensity "$BLOOM_INTENSITY")
fi

RANGE_FLAGS=()
if [ -n "$START_FRAME" ]; then
    RANGE_FLAGS+=(--start-frame "$START_FRAME")
fi
if [ -n "$END_FRAME" ]; then
    RANGE_FLAGS+=(--end-frame "$END_FRAME")
fi
if [ -n "$FPS" ]; then
    RANGE_FLAGS+=(--fps "$FPS")
fi

mkdir -p "$OUT_DIR"

if [ ! -f "scenes/${SCENE}.cfg" ]; then
    echo "Unknown scene: scenes/${SCENE}.cfg" >&2
    exit 1
fi

echo "Rendering ${SCENE} at ${WIDTH}x${HEIGHT}, ${SAMPLES} samples, using the ${RENDERER} renderer..."

./raytracer --path "scenes/${SCENE}.cfg" --cli --renderer "$RENDERER" \
    --width "$WIDTH" --height "$HEIGHT" --samples "$SAMPLES" \
    ${TONE_FLAGS[@]+"${TONE_FLAGS[@]}"} --animation \
    ${RANGE_FLAGS[@]+"${RANGE_FLAGS[@]}"} --output "$PATTERN"

if [ "$FFMPEG" = "1" ]; then
    effective_fps="$FPS"

    if [ -z "$effective_fps" ]; then
        effective_fps="$(sed -n 's/^[[:space:]]*fps[[:space:]]*=[[:space:]]*\([0-9][0-9]*\).*/\1/p' \
            "scenes/${SCENE}.cfg" | head -n 1)"
        effective_fps="${effective_fps:-24}"
    fi

    video="${OUT_DIR}/${SCENE}.mp4"

    if ! command -v ffmpeg >/dev/null 2>&1; then
        echo "ffmpeg is not installed; frames are in ${OUT_DIR}/" >&2
        exit 0
    fi

    ffmpeg -y -framerate "$effective_fps" -i "$PATTERN" \
        -c:v libx264 -pix_fmt yuv420p "$video"
    echo "Video: ${video}"
else
    echo "Frames are in ${OUT_DIR}/"
fi
