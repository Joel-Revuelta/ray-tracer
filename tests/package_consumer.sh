#!/usr/bin/env bash
set -euo pipefail

if [ "$#" -ne 3 ]; then
    echo "usage: $0 <source-dir> <build-dir> <install-prefix>" >&2
    exit 2
fi

source_dir="$(cd "$1" && pwd)"
build_dir="$(cd "$2" && pwd)"
prefix="$3"
consumer_build="${build_dir}/package-consumer"

rm -rf "$prefix" "$consumer_build"

cmake --install "$build_dir" --prefix "$prefix"
cmake -S "${source_dir}/tests/package_consumer" -B "$consumer_build" \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_PREFIX_PATH="$prefix"
cmake --build "$consumer_build" -j"$(nproc)"

output="$(LD_LIBRARY_PATH="$prefix/bin${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}" \
    "$consumer_build/raytracer_package_consumer")"
echo "$output"

if [ "$output" != "3.74166 960 1" ]; then
    echo "unexpected package consumer output" >&2
    exit 1
fi
