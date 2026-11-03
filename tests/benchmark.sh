#!/usr/bin/env bash
set -euo pipefail

if [ "$#" -lt 2 ] || [ "$#" -gt 3 ]; then
    echo "usage: $0 <raytracer> <source-dir> [scene-dir]" >&2
    exit 2
fi

executable="$1"
source_dir="$2"
scene_dir="${3:-${TMPDIR:-/tmp}}"
width="${BENCH_WIDTH:-320}"
height="${BENCH_HEIGHT:-180}"
frames="${BENCH_FRAMES:-4}"
counts="${BENCH_COUNTS:-128 512 2048}"

mkdir -p "${scene_dir}"

renderers="cpu"
if "${executable}" --list-renderers 2>/dev/null | grep -q 'cuda.*\[available\]'; then
    renderers="cpu cuda"
fi

time_render() {
    local renderer="$1"
    local scene="$2"
    local mode="$3"
    local start end

    start="$(date +%s%N)"

    if [ "${mode}" = "linear" ]; then
        RAYTRACER_DISABLE_BVH=1 "${executable}" --path "${scene}" --cli --renderer "${renderer}" \
            --width "${width}" --height "${height}" --frames "${frames}" \
            --output "${scene_dir}/bench-${renderer}-${mode}.ppm" >/dev/null 2>&1
    else
        "${executable}" --path "${scene}" --cli --renderer "${renderer}" \
            --width "${width}" --height "${height}" --frames "${frames}" \
            --output "${scene_dir}/bench-${renderer}-${mode}.ppm" >/dev/null 2>&1
    fi

    end="$(date +%s%N)"
    echo "$(( (end - start) / 1000000 ))"
}

printf '%-6s %-8s %-8s %s\n' "gpu" "spheres" "mode" "milliseconds"

for count in ${counts}; do
    scene="${scene_dir}/stress-${count}.cfg"

    bash "${source_dir}/tests/generate_stress_scene.sh" "${count}" > "${scene}"

    for renderer in ${renderers}; do
        for mode in bvh linear; do
            elapsed="$(time_render "${renderer}" "${scene}" "${mode}")"
            printf '%-6s %-8s %-8s %s\n' "${renderer}" "${count}" "${mode}" "${elapsed}"
        done
    done
done

echo
echo "resolution ${width}x${height}, ${frames} samples"
