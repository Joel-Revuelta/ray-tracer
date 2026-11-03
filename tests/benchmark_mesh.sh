#!/usr/bin/env bash
set -euo pipefail

if [ "$#" -lt 2 ] || [ "$#" -gt 3 ]; then
    echo "usage: $0 <raytracer> <source-dir> [work-dir]" >&2
    exit 2
fi

executable="$1"
source_dir="$2"
work_dir="${3:-${TMPDIR:-/tmp}/raytracer-mesh-bench}"
segments="${BENCH_MESH_SEGMENTS:-64}"
rings="${BENCH_MESH_RINGS:-32}"
width="${BENCH_WIDTH:-320}"
height="${BENCH_HEIGHT:-180}"
frames="${BENCH_FRAMES:-1}"

mkdir -p "${work_dir}"
python3 "${source_dir}/tests/generate_uv_sphere.py" "${segments}" "${rings}" "${work_dir}/sphere.obj"
triangles="$(( segments * rings * 2 ))"

cat > "${work_dir}/scene.cfg" <<'CONFIG'
camera:
{
    position = { x = 0.0; y = 0.0; z = -4.0; };
    rotation = { x = 0.0; y = 0.0; z = 0.0; };
    fieldOfView = 45.0;
};

materials:
{
    surface = {
        type = "diffuse";
        ambientReflection = 0.25;
        diffuseReflection = 0.90;
        color = { r = 0.55; g = 0.65; b = 0.85; };
    };
};

primitives:
{
    meshes = (
        {
            material = 0;
            path = "sphere.obj";
        }
    );
};

lights:
{
    ambient = {
        color = { r = 0.45; g = 0.50; b = 0.65; };
        intensity = 0.30;
    };
    directional = {
        position = { x = -0.35; y = -1.0; z = 0.45; };
        color = { r = 1.0; g = 0.96; b = 0.90; };
        intensity = 0.80;
    };
};
CONFIG

renderers="cpu"
if "${executable}" --list-renderers 2>/dev/null | grep -q 'cuda.*\[available\]'; then
    renderers="cpu cuda"
fi

time_render() {
    local renderer="$1"
    local mode="$2"
    local start end

    start="$(date +%s%N)"

    if [ "${mode}" = "linear" ]; then
        RAYTRACER_DISABLE_BVH=1 "${executable}" --path "${work_dir}/scene.cfg" --cli --renderer "${renderer}" \
            --width "${width}" --height "${height}" --frames "${frames}" \
            --output "${work_dir}/mesh-${renderer}-${mode}.ppm" >/dev/null 2>&1
    else
        "${executable}" --path "${work_dir}/scene.cfg" --cli --renderer "${renderer}" \
            --width "${width}" --height "${height}" --frames "${frames}" \
            --output "${work_dir}/mesh-${renderer}-${mode}.ppm" >/dev/null 2>&1
    fi

    end="$(date +%s%N)"
    echo "$(( (end - start) / 1000000 ))"
}

printf '%-6s %-10s %-8s %s\n' "gpu" "triangles" "mode" "milliseconds"

for renderer in ${renderers}; do
    for mode in bvh linear; do
        elapsed="$(time_render "${renderer}" "${mode}")"
        printf '%-6s %-10s %-8s %s\n' "${renderer}" "${triangles}" "${mode}" "${elapsed}"
    done
done

echo
echo "resolution ${width}x${height}, ${frames} samples, ${segments}x${rings} sphere"
