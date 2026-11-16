#!/usr/bin/env bash
set -euo pipefail

if [ "$#" -ne 3 ]; then
    echo "usage: $0 <raytracer> <source-dir> <build-dir>" >&2
    exit 2
fi

executable="$1"
source_dir="$2"
build_dir="$3"
out_dir="${build_dir}/malformed-corpus"

rm -rf "$out_dir"
mkdir -p "$out_dir"

expect_failure() {
    local name="$1"
    local scene="$2"

    if timeout 10 "$executable" --path "$scene" --cli --renderer cpu \
        --width 16 --height 9 --samples 1 --output "$out_dir/${name}.ppm" \
        >"$out_dir/${name}.log" 2>&1; then
        echo "malformed scene was accepted: ${name}" >&2
        cat "$out_dir/${name}.log" >&2
        return 1
    fi
}

: >"$out_dir/empty.cfg"
expect_failure empty "$out_dir/empty.cfg"

cat >"$out_dir/truncated.cfg" <<'EOF'
camera:
{
    position = { x = 0.0; y = 0.0; z = -5.0; };
    rotation = { x = 0.0; y = 0.0; z = 0.0; };
    fieldOfView = 60.0;

materials:
{
EOF
expect_failure truncated "$out_dir/truncated.cfg"

cat >"$out_dir/wrong-type.cfg" <<'EOF'
camera:
{
    position = { x = 0.0; y = 0.0; z = -5.0; };
    rotation = { x = 0.0; y = 0.0; z = 0.0; };
    fieldOfView = "wide";
};
materials:
{
    only = { type = "diffuse"; ambientReflection = 0.2; diffuseReflection = 0.9; color = { r = 1.0; g = 1.0; b = 1.0; }; };
};
primitives:
{
    spheres = ( { material = 0; r = 1.0; position = { x = 0.0; y = 0.0; z = 0.0; }; } );
};
lights:
{
    ambient = { color = { r = 1.0; g = 1.0; b = 1.0; }; intensity = 0.5; };
};
EOF
expect_failure wrong-type "$out_dir/wrong-type.cfg"

cat >"$out_dir/negative-radius.cfg" <<'EOF'
camera:
{
    position = { x = 0.0; y = 0.0; z = -5.0; };
    rotation = { x = 0.0; y = 0.0; z = 0.0; };
    fieldOfView = 60.0;
};
materials:
{
    only = { type = "diffuse"; ambientReflection = 0.2; diffuseReflection = 0.9; color = { r = 1.0; g = 1.0; b = 1.0; }; };
};
primitives:
{
    spheres = ( { material = 0; r = -1.0; position = { x = 0.0; y = 0.0; z = 0.0; }; } );
};
lights:
{
    ambient = { color = { r = 1.0; g = 1.0; b = 1.0; }; intensity = 0.5; };
};
EOF
expect_failure negative-radius "$out_dir/negative-radius.cfg"

cat >"$out_dir/unknown-material.cfg" <<'EOF'
camera:
{
    position = { x = 0.0; y = 0.0; z = -5.0; };
    rotation = { x = 0.0; y = 0.0; z = 0.0; };
    fieldOfView = 60.0;
};
materials:
{
    only = { type = "unobtainium"; };
};
primitives:
{
    spheres = ( { material = 0; r = 1.0; position = { x = 0.0; y = 0.0; z = 0.0; }; } );
};
lights:
{
    ambient = { color = { r = 1.0; g = 1.0; b = 1.0; }; intensity = 0.5; };
};
EOF
expect_failure unknown-material "$out_dir/unknown-material.cfg"

python3 - "$out_dir/deep-nesting.cfg" <<'PY'
from pathlib import Path
import sys

inner = '''
materials:
{
    only = { type = "diffuse"; ambientReflection = 0.2; diffuseReflection = 0.9; color = { r = 1.0; g = 1.0; b = 1.0; }; };
};
primitives:
{
    spheres = ( { material = 0; r = 1.0; position = { x = 0.0; y = 0.0; z = 0.0; }; } );
};
'''

for _ in range(40):
    inner = 'primitives: { scene: {' + inner + '}; };'

scene = '''camera:
{
    position = { x = 0.0; y = 0.0; z = -5.0; };
    rotation = { x = 0.0; y = 0.0; z = 0.0; };
    fieldOfView = 60.0;
};
materials:
{
    only = { type = "diffuse"; ambientReflection = 0.2; diffuseReflection = 0.9; color = { r = 1.0; g = 1.0; b = 1.0; }; };
};
''' + inner + '''
lights:
{
    ambient = { color = { r = 1.0; g = 1.0; b = 1.0; }; intensity = 0.5; };
};
'''
Path(sys.argv[1]).write_text(scene)
PY
expect_failure deep-nesting "$out_dir/deep-nesting.cfg"

printf 'camera:\n{\n\0garbage\n' >"$out_dir/nul-bytes.cfg"
expect_failure nul-bytes "$out_dir/nul-bytes.cfg"
