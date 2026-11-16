#!/usr/bin/env bash
set -euo pipefail

if [ "$#" -ne 3 ]; then
    echo "usage: $0 <raytracer> <source-dir> <build-dir>" >&2
    exit 2
fi

executable="$1"
source_dir="$2"
build_dir="$3"
out_dir="${build_dir}/atmosphere-regressions"

rm -rf "$out_dir"
mkdir -p "$out_dir"

base_scene="${out_dir}/base.cfg"

cat >"$base_scene" <<'EOF'
camera:
{
    position = { x = 0.0; y = 1.4; z = -6.0; };
    rotation = { x = 0.0; y = 0.8; z = 0.0; };
    fieldOfView = 52.0;
};
background:
{
    zenith = { r = 0.02; g = 0.03; b = 0.07; };
    horizon = { r = 0.12; g = 0.15; b = 0.28; };
    ground = { r = 0.01; g = 0.01; b = 0.02; };
    intensity = 1.0;
};
materials:
{
    floor = { type = "diffuse"; ambientReflection = 0.2; diffuseReflection = 0.9; color = { r = 0.3; g = 0.32; b = 0.38; }; };
    ball = { type = "diffuse"; ambientReflection = 0.2; diffuseReflection = 0.9; color = { r = 0.8; g = 0.25; b = 0.15; }; };
};
primitives:
{
    planes = ( { material = 0; position = { x = 0.0; y = 0.0; z = 0.0; }; normal = { x = 0.0; y = 1.0; z = 0.0; }; } );
    spheres = ( { material = 1; r = 1.0; position = { x = 0.0; y = 1.0; z = 0.0; }; } );
};
lights:
{
    ambient = { color = { r = 0.5; g = 0.6; b = 1.0; }; intensity = 0.2; };
    point = ( { position = { x = -3.0; y = 4.0; z = -2.0; }; color = { r = 1.0; g = 0.85; b = 0.6; }; intensity = 2.4; attenuation = { constant = 1.0; linear = 0.05; quadratic = 0.002; }; } );
};
EOF

zero_scene="${out_dir}/zero-density.cfg"
{
    cat "$base_scene"
    cat <<'EOF'
atmosphere:
{
    type = "homogeneous";
    density = 0.0;
    color = { r = 0.5; g = 0.6; b = 0.9; };
    anisotropy = 0.4;
    maxDistance = 40.0;
};
EOF
} >"$zero_scene"

homogeneous_scene="${out_dir}/homogeneous.cfg"
{
    cat "$base_scene"
    cat <<'EOF'
atmosphere:
{
    type = "homogeneous";
    density = 0.09;
    color = { r = 0.65; g = 0.78; b = 1.0; };
    anisotropy = 0.4;
    maxDistance = 40.0;
};
EOF
} >"$homogeneous_scene"

height_scene="${out_dir}/height.cfg"
{
    cat "$base_scene"
    cat <<'EOF'
atmosphere:
{
    type = "height";
    density = 0.12;
    color = { r = 0.60; g = 0.82; b = 0.95; };
    anisotropy = 0.2;
    groundY = 0.0;
    heightFalloff = 0.4;
    maxDistance = 40.0;
};
EOF
} >"$height_scene"

render() {
    local scene="$1"
    local output="$2"

    if ! "$executable" --path "$scene" --cli --renderer cpu \
        --width 64 --height 36 --samples 16 --seed 5 \
        --output "$output" >"${output%.ppm}.log" 2>&1; then
        cat "${output%.ppm}.log" >&2
        return 1
    fi
}

render "$base_scene" "${out_dir}/base.ppm"
render "$zero_scene" "${out_dir}/zero.ppm"

if ! cmp -s "${out_dir}/base.ppm" "${out_dir}/zero.ppm"; then
    echo "zero-density atmosphere changed a scene without atmosphere" >&2
    exit 1
fi

for scene in homogeneous height; do
    render "${out_dir}/${scene}.cfg" "${out_dir}/${scene}-1.ppm"
    render "${out_dir}/${scene}.cfg" "${out_dir}/${scene}-2.ppm"

    if ! cmp -s "${out_dir}/${scene}-1.ppm" "${out_dir}/${scene}-2.ppm"; then
        echo "${scene} fog rendering is not deterministic" >&2
        exit 1
    fi
done
