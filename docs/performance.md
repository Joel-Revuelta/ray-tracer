# Performance

The renderer builds a binned-SAH BVH per scene and one per unique mesh asset.
This page records the methodology and reference numbers for the scale
milestones.

## Reproduce

```bash
cmake -S . -B build
cmake --build build -j"$(nproc)"

# Wall-clock comparison; prints a table.
BENCH_COUNTS="128 512 2048" bash tests/benchmark.sh ./raytracer . /tmp/bench
```

The benchmark generates deterministic sphere grids with
`tests/generate_stress_scene.sh`, renders them at `320x180` with four samples,
and runs each configuration once with the BVH enabled and once with
`RAYTRACER_DISABLE_BVH=1`.

To inspect the build itself:

```bash
RAYTRACER_BVH_STATS=1 ./raytracer --path scenes/showcase.cfg \
    --cli --renderer cpu --width 64 --height 36 --frames 1 --output /tmp/out.ppm
```

## Reference machine

- CPU: Intel Core i7-14700HX, 28 hardware threads
- GPU: NVIDIA GeForce RTX 4070 Laptop GPU, CUDA 13.4, `sm_89`
- Build: Release, `-Werror -Wall -Wextra`
- Render: `320x180`, 4 samples, one frame per configuration

## Results

CPU (`tests/benchmark.sh`, wall clock including startup and parsing):

| Spheres | Linear | BVH | Speedup |
| ---: | ---: | ---: | ---: |
| 128 | 247 ms | 163 ms | 1.5x |
| 512 | 708 ms | 177 ms | 4.0x |
| 2048 | 2797 ms | 202 ms | 13.8x |

CUDA (same scenes; startup and host-side BVH upload dominate small counts):

| Spheres | Linear | BVH | Speedup |
| ---: | ---: | ---: | ---: |
| 128 | ~223 ms | ~279 ms | ~1.0x (noise) |
| 512 | 234 ms | 211 ms | 1.1x |
| 2048 | 355 ms | 321 ms | 1.1x |

For 512 bounded spheres the builder reports roughly 339 nodes and a build time
around `0.3 ms` on the reference machine.

## Mesh benchmark

```bash
BENCH_MESH_SEGMENTS=64 BENCH_MESH_RINGS=32 \
    bash tests/benchmark_mesh.sh ./raytracer . /tmp/mesh-bench
```

The script generates a UV sphere, writes a temporary scene, and compares the
mesh BVH against a linear triangle scan.

Default run (`4096` triangles, `320x180`, 1 sample):

| Renderer | Linear | BVH | Speedup |
| --- | ---: | ---: | ---: |
| CPU | 439 ms | 38 ms | 11.5x |
| CUDA | 228 ms | 298 ms | startup-dominated |

Larger run (`16384` triangles):

| Renderer | Settings | Linear | BVH | Speedup |
| --- | --- | ---: | ---: | ---: |
| CPU | `320x180`, 1 sample | 1802 ms | 49 ms | 36.8x |
| CUDA | `640x360`, 8 samples | 715 ms | 335 ms | 2.1x |

At very small workloads CUDA is dominated by plugin loading, host-side BVH
construction and the first upload; the mesh BVH becomes worthwhile at higher
resolution/sample counts.

## Notes

- These are single-run wall-clock numbers. Treat small differences, especially
  on CUDA where launch and transfer costs dominate, as noise.
- The CPU numbers include plugin loading and scene parsing. The parse-time BVH
  build is part of the measured time.
- BVH and linear traversal can disagree on a handful of pixels when two
  surfaces are exactly coincident, because the visit order decides the winner.
  `tests/bvh_parity.sh` uses a scene without coincident surfaces and requires a
  byte-identical PPM. The difference is negligible for ordinary scenes.
- Small scenes (fewer than eight bounded primitives) intentionally use a linear
  scan. The same threshold applies to meshes with fewer than eight triangles.
- Mesh BVHs are built once per unique asset at load time and shared by every
  instance.
