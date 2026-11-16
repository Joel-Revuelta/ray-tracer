#!/usr/bin/env bash
set -euo pipefail

if [ "$#" -ne 3 ]; then
    echo "usage: $0 <raytracer> <source-dir> <build-dir>" >&2
    exit 2
fi

executable="$1"
source_dir="$2"
build_dir="$3"
out_dir="${build_dir}/scene-animation"

rm -rf "$out_dir"
mkdir -p "$out_dir"

render() {
    local output="$1"

    shift
    "$executable" --path "$source_dir/tests/scene_animation.cfg" --cli --renderer cpu \
        --width 96 --height 54 --samples 2 "$@" --output "$output" >/dev/null
}

render "$out_dir/first-%02d.ppm" --animation --start-frame 24 --end-frame 24
render "$out_dir/second-%02d.ppm" --animation --start-frame 24 --end-frame 24
cmp "$out_dir/first-24.ppm" "$out_dir/second-24.ppm"

render "$out_dir/start-%02d.ppm" --animation --start-frame 0 --end-frame 0
if cmp -s "$out_dir/start-00.ppm" "$out_dir/first-24.ppm"; then
    echo "scene animation produced identical start and middle frames" >&2
    exit 1
fi

# A still render must ignore the scene animation block.
"$executable" --path "$source_dir/tests/scene_animation.cfg" --cli --renderer cpu \
    --width 96 --height 54 --samples 2 --output "$out_dir/still.ppm" >/dev/null

python3 - "$out_dir/first.json" <<'PY'
import json
import sys

with open(sys.argv[1], encoding="utf-8") as handle:
    data = json.load(handle)

scene = data.get("sceneAnimation")
assert scene is not None, data
assert scene["instances"] == ["mover"], scene
assert scene["lights"] == ["pulse"], scene
PY
