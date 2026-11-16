#!/usr/bin/env bash
set -euo pipefail

if [ "$#" -ne 3 ]; then
    echo "usage: $0 <raytracer> <source-dir> <build-dir>" >&2
    exit 2
fi

executable="$1"
source_dir="$2"
build_dir="$3"
report="${build_dir}/benchmark-report.csv"

printf 'case,renderer,width,height,samples,milliseconds\n' >"$report"

measure() {
    local case_name="$1"
    local renderer="$2"
    local width="$3"
    local height="$4"
    local samples="$5"
    local scene="$6"

    shift 6
    local start end elapsed

    start="$(date +%s%N)"
    "$executable" --path "$source_dir/scenes/${scene}.cfg" --cli --renderer "$renderer" \
        --width "$width" --height "$height" --samples "$samples" --seed 5 \
        "$@" --output "$build_dir/benchmark-${case_name}-${renderer}.ppm" >/dev/null 2>&1
    end="$(date +%s%N)"
    elapsed=$(( (end - start) / 1000000 ))
    printf '%s,%s,%s,%s,%s,%s\n' "$case_name" "$renderer" "$width" "$height" "$samples" "$elapsed" >>"$report"
}

measure still cpu 320 180 32 showcase
measure animation cpu 320 180 16 brass_orrery --animation --start-frame 96 --end-frame 96
measure motion-blur cpu 320 180 16 brass_orrery --animation --start-frame 96 --end-frame 96 \
    --shutter-angle 180 --shutter-samples 4

if "$executable" --list-renderers 2>/dev/null | grep -q 'cuda.*\[available\]'; then
    measure still cuda 320 180 32 showcase
    measure animation cuda 320 180 16 brass_orrery --animation --start-frame 96 --end-frame 96
    measure motion-blur cuda 320 180 16 brass_orrery --animation --start-frame 96 --end-frame 96 \
        --shutter-angle 180 --shutter-samples 4
fi

cat "$report"
