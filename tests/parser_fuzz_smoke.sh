#!/usr/bin/env bash
set -euo pipefail

if [ "$#" -ne 3 ]; then
    echo "usage: $0 <raytracer> <source-dir> <build-dir>" >&2
    exit 2
fi

executable="$1"
source_dir="$2"
build_dir="$3"
out_dir="${build_dir}/parser-fuzz"

rm -rf "$out_dir"
mkdir -p "$out_dir"

python3 - "$source_dir" "$out_dir" <<'PY'
from pathlib import Path
import random
import sys

source = Path(sys.argv[1])
out = Path(sys.argv[2])
seeds = [
    source / "scenes/mtl_materials.cfg",
    source / "scenes/material_maps.cfg",
    source / "scenes/brass_orrery.cfg",
    source / "tests/mixed_material.cfg",
]
rng = random.Random(20261116)
count = 0

for seed in seeds:
    data = seed.read_bytes()
    for case in range(20):
        mutated = bytearray(data)
        for _ in range(rng.randint(1, 6)):
            operation = rng.choice(("replace", "insert", "delete"))
            if operation == "replace" and mutated:
                mutated[rng.randrange(len(mutated))] = rng.randrange(32, 127)
            elif operation == "insert":
                mutated.insert(rng.randrange(len(mutated) + 1), rng.randrange(32, 127))
            elif operation == "delete" and mutated:
                del mutated[rng.randrange(len(mutated))]
        (out / f"case-{count:04d}.cfg").write_bytes(bytes(mutated))
        count += 1
PY

status=0

for scene in "$out_dir"/*.cfg; do
    set +e
    timeout 5 "$executable" --path "$scene" --cli --renderer cpu --validate \
        >"${scene%.cfg}.log" 2>&1
    result=$?
    set -e

    if [ "$result" -eq 124 ]; then
        echo "parser timed out on ${scene}" >&2
        cat "${scene%.cfg}.log" >&2
        status=1
    elif [ "$result" -ge 128 ]; then
        echo "parser crashed on ${scene} (exit ${result})" >&2
        cat "${scene%.cfg}.log" >&2
        status=1
    fi
done

exit "$status"
