#!/usr/bin/env bash
set -euo pipefail

if [ "$#" -ne 3 ]; then
    echo "usage: $0 <raytracer> <source-dir> <build-dir>" >&2
    exit 2
fi

executable="$1"
source_dir="$2"
build_dir="$3"
out_dir="${build_dir}/video-output"
ffmpeg="${RAYTRACER_FFMPEG:-$(command -v ffmpeg || true)}"

if [ -z "$ffmpeg" ] || [ ! -x "$ffmpeg" ]; then
    echo "ffmpeg is unavailable; skipping video output test"
    exit 77
fi

ffprobe="$(dirname "$ffmpeg")/ffprobe"

if [ ! -x "$ffprobe" ]; then
    ffprobe="$(command -v ffprobe || true)"
fi
if [ -z "$ffprobe" ] || [ ! -x "$ffprobe" ]; then
    echo "ffprobe is unavailable; skipping video output test"
    exit 77
fi

export RAYTRACER_FFMPEG="$ffmpeg"

rm -rf "$out_dir"
mkdir -p "$out_dir"

render() {
    local log="$1"

    shift
    if ! "$executable" --path "$source_dir/scenes/brass_orrery.cfg" --cli --renderer cpu \
        "$@" >"$log" 2>&1; then
        cat "$log" >&2
        return 1
    fi
}

probe() {
    local file="$1"
    local expected_width="$2"
    local expected_height="$3"
    local expected_frames="$4"
    local expected_codec="$5"
    local width height frames rate codec

    width="$("$ffprobe" -v error -select_streams v:0 -show_entries stream=width -of default=nw=1:nk=1 "$file")"
    height="$("$ffprobe" -v error -select_streams v:0 -show_entries stream=height -of default=nw=1:nk=1 "$file")"
    rate="$("$ffprobe" -v error -select_streams v:0 -show_entries stream=r_frame_rate -of default=nw=1:nk=1 "$file")"
    codec="$("$ffprobe" -v error -select_streams v:0 -show_entries stream=codec_name -of default=nw=1:nk=1 "$file")"
    frames="$("$ffprobe" -v error -count_frames -select_streams v:0 \
        -show_entries stream=nb_read_frames -of default=nw=1:nk=1 "$file")"

    if [ "$width" != "$expected_width" ] || [ "$height" != "$expected_height" ] ||
        [ "$frames" != "$expected_frames" ] || [ "$codec" != "$expected_codec" ] ||
        [ "$rate" != "24/1" ]; then
        echo "unexpected video properties for $file: ${width}x${height}, ${frames} frames, ${rate} fps, $codec" >&2
        return 1
    fi
}

mp4="${out_dir}/orrery.mp4"

render "$out_dir/mp4.log" --width 64 --height 36 --samples 2 \
    --animation --start-frame 0 --end-frame 5 \
    --shutter-angle 180 --shutter-samples 2 \
    --video "$mp4"

if [ ! -s "$mp4" ]; then
    echo "MP4 video was not written" >&2
    exit 1
fi
probe "$mp4" 64 36 6 h264

python3 - "$out_dir/orrery.json" "$mp4" <<'PY'
import json
import sys

with open(sys.argv[1], encoding="utf-8") as handle:
    data = json.load(handle)

video = data.get("video")
assert video is not None, data
assert video["path"] == sys.argv[2], video
assert video["codec"] == "h264", video
assert len(data["frames"]) == 6, data["frames"]
assert all(frame["rendered"] for frame in data["frames"]), data["frames"]
assert all(frame["file"] == "" for frame in data["frames"]), data["frames"]
PY

scene_video="${out_dir}/scene-animation.mp4"

if ! "$executable" --path "$source_dir/tests/scene_animation.cfg" --cli --renderer cpu \
    --width 32 --height 18 --samples 1 --animation --start-frame 0 --end-frame 3 \
    --video "$scene_video" >"$out_dir/scene-video.log" 2>&1; then
    cat "$out_dir/scene-video.log" >&2
    exit 1
fi
probe "$scene_video" 32 18 4 h264

if "$ffmpeg" -hide_banner -encoders 2>/dev/null | grep -q 'libvpx-vp9'; then
    webm="${out_dir}/orrery.webm"

    render "$out_dir/webm.log" --width 64 --height 36 --samples 1 \
        --animation --start-frame 0 --end-frame 3 \
        --video "$webm"
    probe "$webm" 64 36 4 vp9
fi

both_mp4="${out_dir}/both.mp4"
both_frames="${out_dir}/frames/f-%02d.ppm"

mkdir -p "$out_dir/frames"
render "$out_dir/both.log" --width 32 --height 18 --samples 1 \
    --animation --start-frame 0 --end-frame 1 \
    --video "$both_mp4" --output "$both_frames"

if [ ! -s "$both_mp4" ] || [ ! -s "$out_dir/frames/f-00.ppm" ] || [ ! -s "$out_dir/frames/f-01.ppm" ]; then
    echo "combined video and frame output is incomplete" >&2
    exit 1
fi

python3 - "$out_dir/frames/f.json" "$both_mp4" <<'PY'
import json
import sys

with open(sys.argv[1], encoding="utf-8") as handle:
    data = json.load(handle)

assert data["video"]["path"] == sys.argv[2], data["video"]
assert all(frame["file"] for frame in data["frames"]), data["frames"]
PY

# Odd dimensions are padded to even output dimensions for yuv420p.
odd="${out_dir}/odd.mp4"

render "$out_dir/odd.log" --width 63 --height 35 --samples 1 \
    --animation --start-frame 0 --end-frame 1 \
    --video "$odd"
probe "$odd" 64 36 2 h264

# Validation reports the resolved video settings without rendering.
if ! "$executable" --path "$source_dir/scenes/brass_orrery.cfg" --cli \
    --width 64 --height 36 --samples 1 --animation --start-frame 0 --end-frame 1 \
    --video "$out_dir/validate.mp4" --validate >"$out_dir/validate.log" 2>&1; then
    cat "$out_dir/validate.log" >&2
    exit 1
fi
if ! grep -q 'Video: .*h264' "$out_dir/validate.log"; then
    echo "validation did not report the video settings" >&2
    exit 1
fi
if [ -e "$out_dir/validate.mp4" ]; then
    echo "validation unexpectedly rendered a video" >&2
    exit 1
fi

# Incompatible option combinations must fail.
if "$executable" --path "$source_dir/scenes/brass_orrery.cfg" --cli \
    --animation --video "$out_dir/resume.mp4" --resume >"$out_dir/resume.log" 2>&1; then
    echo "resume with video was accepted" >&2
    exit 1
fi
if "$executable" --path "$source_dir/scenes/brass_orrery.cfg" --cli \
    --animation --output "$out_dir/output.mp4" >"$out_dir/output.log" 2>&1; then
    echo "video extension in --output was accepted" >&2
    exit 1
fi
if RAYTRACER_FFMPEG=/nonexistent "$executable" --path "$source_dir/scenes/brass_orrery.cfg" --cli \
    --animation --end-frame 0 --video "$out_dir/missing.mp4" >"$out_dir/missing.log" 2>&1; then
    echo "missing ffmpeg was accepted" >&2
    exit 1
fi

if find "$out_dir" -name '*.tmp.*' | grep -q .; then
    echo "temporary video files were left behind" >&2
    exit 1
fi
