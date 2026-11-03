# Developer Guide

This guide covers the build, architecture, plugin model, coding conventions and
verification workflow for the ray tracer.

## Overview

The project is split into a shared core, a thin executable and runtime-loaded
plugins:

- `raytracer_core` contains the camera, scene graph, parser, plugin loader,
  renderer interfaces and shared render helpers.
- `raytracer` contains `main.cpp` and `App.cpp`; it parses arguments, builds the
  scene and selects a renderer.
- `plugins/*.so` contains the concrete primitives, materials, lights, GUI and
  renderers.

All plugins link against `raytracer_core`. This gives the executable and every
plugin one definition of shared types and one set of RTTI symbols, which is
required for safe `std::shared_ptr` and interface use across shared libraries.

Transformation plugins are the exception: they currently compile the concrete
primitive sources as well so their `dynamic_cast` dispatch can see those types.
Keep primitive layouts in sync until that dispatch is replaced with virtual
hooks.

## Prerequisites

- CMake 3.20+ (tested with 4.2.3)
- C++20 compiler (tested with GCC 15.2)
- `libconfig++` development package
- Optional SFML 3.0.x development package (Graphics, Window, System)
- Optional GLFW 3.3+ and GLEW 2.x development packages for the OpenGL GUI
- Optional CUDA Toolkit 13.x (tested with 13.4)
- Optional Python 3 + Pillow for the scene render scripts

The CUDA renderer compiles for compute capability `89` (Ada / RTX 4070 Laptop)
by default. Set `-DRAYTRACER_CUDA_ARCH=<cc>` for a different GPU.

## Build

```bash
cmake -S . -B build
cmake --build build -j"$(nproc)"
```

Useful configurations:

```bash
# CPU-only build
cmake -S . -B build -DRAYTRACER_ENABLE_CUDA=OFF

# Without the GLFW/OpenGL GUI plugin
cmake -S . -B build -DRAYTRACER_ENABLE_OPENGL_GUI=OFF

# Explicit CUDA toolkit
cmake -S . -B build -DCUDAToolkit_ROOT=/usr/local/cuda-13.4

# Reconfigure an existing build directory
cmake -S . -B build
cmake --build build -j"$(nproc)"
```

The executable is written to the repository root. Plugins are written to
`plugins/`, and `libraytracer_core.so` stays in `build/`. Run the executable from
the repository root so that `./plugins/` resolves.

## Repository layout

| Path | Purpose |
| --- | --- |
| `src/main.cpp` | Program entry point. |
| `src/App.cpp` | CLI parsing, scene/camera setup, renderer selection. |
| `src/Camera.cpp` | Camera basis, viewport and pixel ray setup. |
| `src/scene/` | `Scene` and `SceneBuilder`. |
| `src/plugins/parser/` | libconfig scene parser. |
| `src/plugins/primitives/` | Sphere, plane, cylinder, cone + exported `create` functions. |
| `src/plugins/materials/` | Diffuse, metallic, emissive, flat and custom materials. |
| `src/plugins/lights/` | Ambient, directional and point lights. |
| `src/plugins/renderers/CPU/` | CPU renderer plugin. |
| `src/plugins/renderers/CUDA/` | CUDA host plugin and device kernels. |
| `src/plugins/GUI/` | SFML 3 and GLFW/OpenGL GUI plugins. |
| `src/render/` | Renderer registry, PPM writer, camera controller, scene snapshot. |
| `include/plugins/renderers/` | Renderer interface, snapshot data, registry, PPM and camera controller headers. |
| `scenes/` | Scene files, render scripts and scene documentation. |
| `screenshots/` | Generated renders and previews. |
| `docs/` | Developer, renderer, CLI and scene-format documentation. |

## Runtime architecture

### Core library

`raytracer_core` is built from:

- `src/utils/*.cpp`
- `src/Camera.cpp`
- `src/scene/*.cpp`
- `src/DLManager/*.cpp`
- `src/factory/*.cpp`
- `src/plugins/parser/*.cpp`
- `src/render/*.cpp`

It links `libconfig++`, `dl` and the platform thread library. Keep new shared
runtime code here, not in the executable, if plugins need it.

### Executable

`App` uses `Parser` to build a `Scene` and `Camera`. It then:

1. Applies the requested render width/height to the camera.
2. Looks up the requested renderer in `RendererRegistry`.
3. Creates the renderer plugin through `Factory`.
4. Checks `IRenderer::isAvailable()`.
5. Builds a `RenderContext` with the scene, camera, optional GUI and options.
6. Calls `IRenderer::run()`.

### Plugin loading

`Factory` maps a logical type name (for example `Sphere`, `DiffuseMaterial`,
`RendererCPU`) to a shared library name. `DLManager`:

- `dlopen`s libraries from `./plugins/`,
- caches open handles,
- resolves the exported `create` symbol with `dlsym`,
- closes all libraries at shutdown.

Every plugin exports a C-linkage factory:

```cpp
extern "C" std::shared_ptr<RayTracer::SomeInterface> create(/* args */);
```

The return type and argument list must match what `Factory::create<T>` expects.
Object parameters such as `Vec3D` and `Color3D` are passed by value in the
exported signature. Primitive, material, light and renderer plugins follow this
pattern.

### Scene and parser

`Parser` reads the libconfig file and uses `SceneBuilder` to assemble:

- `Scene` materials (order defines material indices),
- primitives,
- lights,
- nested scenes (materials appended, primitive indices shifted, lights merged).

The parser is strict. Invalid dimensions, zero normals/directions, bad camera
settings, unknown material types and out-of-range material indices throw and
cause a non-zero exit before an image is written.

See [scene-format.md](scene-format.md) for the complete format.

### Acceleration and transforms

`src/scene/BVHBuilder.cpp` implements the single acceleration builder used by
both renderers. It performs a binned-SAH split over `PrimitiveData`, falls back
to a median split for degenerate distributions, and returns a flat node array
plus primitive indices. Planes and primitives without bounds are returned in a
separate `unbounded` list and tested linearly.

- Scenes with fewer than eight bounded primitives stay linear.
- CPU traversal visits the nearer child first and skips the far child when its
  entry distance is beyond the current hit.
- The CUDA renderer adapts the same build result to `Cuda::BVHNode` and uses a
  near-first stack during traversal.
- `RAYTRACER_DISABLE_BVH=1` forces the linear path for benchmarks and parity
  tests. `RAYTRACER_BVH_STATS=1` prints the bounded/unbounded counts, node count
  and build time to stderr.

Primitive transforms are stored in the device-friendly `TransformData`
(`include/scene/TransformData.hpp`). `InstancePrimitive`
(`include/scene/InstancePrimitive.hpp`) wraps a primitive, transforms incoming
rays into local space, delegates to the plugin, and converts the hit position
and normal back to world space. `IPrimitive::getData()` composes the instance
transform with the child's description, so the CPU BVH, the CUDA snapshot and
both renderers see the same world-space primitive. The legacy
`Rotation`/`Translation` plugins remain available but the parser uses the
data-driven transform path.

Mesh geometry is an immutable `MeshAsset`
(`include/scene/Mesh.hpp`) holding local-space triangles, a local AABB and a
local triangle BVH built with the same `buildBVHFromItems()` helper. The parser
caches assets by resolved OBJ path, so repeated `primitives.meshes` entries
share geometry and differ only by material and `InstancePrimitive` transform.
`MeshPrimitive` implements `IPrimitive`; `getMeshAsset()` exposes the shared
asset to the snapshot builder. The CPU traversal is two-level: the scene BVH
finds the mesh instance, then `MeshPrimitive` traverses its triangle BVH.

For CUDA, `buildSceneSnapshot()` deduplicates assets by pointer and stores
`shared_ptr<const MeshAsset>` in the snapshot, so camera moves do not copy
triangle data. `CudaRenderSession` flattens every asset's triangles, BVH nodes
and indices into global buffers once, offsets the node/index references per
mesh, and stores a `MeshRange` per asset. `Cuda::Primitive::meshIndex` selects
the range; device traversal is scene BVH → mesh BVH → Möller–Trumbore.

`Triangle` stores three positions and three vertex normals. The OBJ loader
fills them from `vn` data and falls back to the face normal when a face has no
normal. `HitPayload` carries both a geometric and a shading normal:
`setHitNormal()` also seeds the shading normal, and smooth meshes override it
with `setShadingNormal()`. `InstancePrimitive` transforms both normals with the
inverse-transpose. Renderers use the shading normal for diffuse/metallic
sampling and lighting, and the geometric normal for ray offsets, termination
checks and dielectric reflection/refraction. CUDA propagates both normals
through the intersection chain (`bestNormal`, `bestShadingNormal`).

### Renderer interface

Renderers implement `RayTracer::IRenderer` from
`include/plugins/renderers/IRenderer.hpp`:

```cpp
struct RenderOptions
{
    int width;
    int height;
    int frames;
    int threads;   // CPU only, 0 = automatic
    int device;    // CUDA only
    uint32_t seed;
    std::string outputPath;
    PostProcessSettings postProcess;
};

struct RenderContext
{
    std::shared_ptr<Scene> scene;
    std::shared_ptr<Camera> camera;
    std::shared_ptr<IGUI> gui;
    RenderOptions options;
};

class IRenderer
{
  public:
    virtual ~IRenderer() = default;
    virtual std::string getName() const = 0;
    virtual std::string getDescription() const = 0;
    virtual bool isAvailable(std::string &reason) const = 0;
    virtual void run(const RenderContext &context) = 0;
};
```

- `getName()` returns the user-facing name (`cpu`, `cuda`).
- `isAvailable()` must return `false` with a reason when the runtime/device is
  missing.
- `run()` performs the render and writes the PPM when requested.
- The CPU renderer walks the plugin-backed scene graph directly.
- The CUDA renderer uses `buildSceneSnapshot(scene, camera)` to obtain a flat,
  device-friendly representation.

See [renderers.md](renderers.md) for the renderer plugin guide, CUDA details and
the flat scene data model.

### GUI

`IGUI` is implemented by the SFML plugin (`--gui sfml` / `--sfml`) and the
GLFW/OpenGL plugin (`--gui opengl`). Renderers receive an optional
`shared_ptr<IGUI>` in `RenderContext`. Renderers call
`CameraController::captureInitial()` once before interactive rendering so `R`
can restore the starting pose.

The OpenGL backend uses GLFW for the window and input, GLEW for modern GL entry
points, and an OpenGL 3.3 core context. Frames are uploaded as an RGBA texture
and rendered through a fullscreen triangle; the viewport preserves the image
aspect ratio with letterboxing. GLFW is forced to the X11 platform so GLEW can
use GLX; a working X11/GLX display is required.

The shared `CameraController` implements:

- `W`/`S`, `A`/`D`: camera-relative forward/back and strafe,
- `Q`/`E`: world-space up/down,
- arrow keys or `I`/`J`/`K`/`L`: look up/left/down/right,
- `+`/`-`: field-of-view zoom,
- `,`/`.`: decrease/increase the lens radius,
- `;`/`'`: pull/push the focus distance,
- `R`: restore the initial position, target, FOV, lens radius and focus distance,
- `X`: lock the look-at point,
- `C`: lock the camera position,
- `Esc`: close the window.

Renderer-level actions:

- `Space`: pause/resume sampling,
- `P`: write the current frame as a timestamped PPM,
- `[` / `]`: adjust exposure and re-tonemap the current accumulation.

`IGUI::setTitle()` lets renderers display progress, exposure, and lens/focus
values. The CUDA renderer polls window events while each GPU kernel is in flight
and uses one-sample batches; keep that behavior if you modify it.

### Camera and depth of field

`Camera` stores `lensRadius` and `focusDistance` alongside the usual pose and
field of view. `initialize()` keeps an explicitly configured focus distance
across resolution changes and camera moves; it only falls back to the look-at
distance when the focus distance is still zero.

Both renderers generate camera rays in the same order:

1. pixel-centre direction plus pixel jitter,
2. if `lensRadius > 0`, two lens samples,
3. a point on the lens disk in the camera's right/up basis,
4. a direction aimed at the focus plane.

A zero lens radius skips step 2 entirely, so pinhole scenes remain
byte-identical to earlier versions. CPU and CUDA share the same PCG sequence
and polar disk mapping; keep that order if you change either path.

`CameraData`/`Cuda::Camera` carry `viewRight`, `viewUp`, `lensRadius`,
`focusDistance` and `focalDistance` to the device.

## Adding components

### Primitive

1. Add `include/plugins/primitives/MyPrimitive.hpp` and
   `src/plugins/primitives/MyPrimitive.cpp`.
2. Derive from `IPrimitive` and implement:
   - `hit(const Ray &, HitPayload &, size_t)`
   - `getData(PrimitiveData &)` for GPU/flat renderers
   - `getMaterialIndex()` / `setMaterialIndex()`
   - `doTransformation()` (or intentionally ignore it)
   - `clone()` so nested-scene merging can copy the primitive safely
   - implement `hit()` in local space; the parser wraps primitives with a
     `transform` block in `InstancePrimitive`, which handles world-space rays,
     hit positions and normals
3. Add an `ExternMyPrimitive.cpp` with the exported `create` function.
4. Add the plugin target in `CMakeLists.txt` and link `raytracer_core`.
5. Add the type and library to `Factory` in `src/factory/Factory.cpp`.
6. Add parser support in `src/plugins/parser/Parser.cpp` and validation rules.
7. Document the new scene fields in [scene-format.md](scene-format.md).

Use the existing sphere/plane/cylinder/cone implementations as references.

### Material or light

1. Add the class under `include/plugins/materials` or
   `include/plugins/lights`.
2. Implement the interface. Lights must implement `getData(LightData &)` for the
   CUDA renderer. Implement `clone()` for materials and lights that should be
   copied independently when nested scenes are merged.
3. Export `create` from a plugin source.
4. Add a CMake target and `Factory` mapping.
5. Add parser support and validation.
6. Update the scene format documentation.

### Renderer

1. Add `src/plugins/renderers/MyRenderer/`.
2. Implement `IRenderer` and export `create()`.
3. Add a CMake target linking `raytracer_core`, and output it to `plugins/`.
4. Add the registry entry in `src/render/RendererRegistry.cpp` and the library
   mapping in `src/factory/Factory.cpp`.
5. Implement availability checks and error reporting.
6. Update the root README, [renderers.md](renderers.md) and the CLI docs.

### GUI backend

1. Add a class under `src/plugins/GUI/` implementing `IGUI`.
2. Export `extern "C" std::shared_ptr<IGUI> create(int width, int height)`.
3. Add a CMake target linking `raytracer_core` and the backend libraries.
4. Add the backend key to `src/factory/Factory.cpp`.
5. Add the backend name to `App::normalizeGuiName()` and the CLI docs.
6. Upload rendered RGBA frames through `setImage()`; keep `getEvent()` mapping
   compatible with `CameraController`.

## Coding conventions

- C++20, 4-space indentation, Allman braces.
- Keep lines around 120 characters.
- Use `namespace RayTracer` and include guards (`SOMETHING_HPP_`).
- Prefer `std::shared_ptr` for scene/plugin objects and `std::unique_ptr` for
  local ownership.
- Throw `std::invalid_argument` / `std::runtime_error` for recoverable setup and
  render errors. `App` catches `std::exception` and exits cleanly.
- Keep renderer-neutral data in `include/plugins/renderers/`.
- Do not duplicate core source files into plugin targets; link
  `raytracer_core`.
- Do not add `-Wno-*` flags to silence warnings; fix the warning.

The project builds with `-Werror -Wall -Wextra`. A clean build is required before
submitting changes.

## Verification

Build:

```bash
cmake -S . -B build
cmake --build build -j"$(nproc)"
```

Renderer discovery:

```bash
./raytracer --list-renderers
```

CPU smoke test:

```bash
./raytracer --path scenes/showcase.cfg --cli --renderer cpu \
  --width 320 --height 180 --frames 8 --output /tmp/cpu.ppm
```

CUDA smoke test:

```bash
./raytracer --path scenes/showcase.cfg --cli --renderer cuda \
  --width 320 --height 180 --frames 8 --output /tmp/cuda.ppm
```

BVH and transform checks:

```bash
bash tests/bvh_parity.sh ./raytracer . build
./raytracer --path scenes/transforms.cfg --cli --renderer cpu \
  --width 320 --height 180 --frames 8 --output /tmp/transforms.ppm
RAYTRACER_BVH_STATS=1 ./raytracer --path scenes/transforms.cfg --cli \
  --renderer cpu --width 64 --height 36 --frames 1 --output /tmp/stats.ppm
```

Mesh checks:

```bash
./raytracer --path scenes/meshes.cfg --cli --renderer cpu \
  --width 320 --height 180 --frames 8 --output /tmp/meshes.ppm
bash tests/mesh_parity.sh ./raytracer . build
```

Depth-of-field check:

```bash
./raytracer --path scenes/depth_of_field.cfg --cli --renderer cpu \
  --width 320 --height 180 --frames 32 --output /tmp/dof.ppm
./raytracer --path scenes/depth_of_field.cfg --cli --renderer cpu \
  --width 320 --height 180 --frames 32 --lens-radius 0 \
  --focus-distance 12 --output /tmp/pinhole.ppm
bash tests/deterministic_dof.sh ./raytracer . build
```

See [performance.md](performance.md) for the benchmark scripts and reference
numbers.

CUDA memory checking:

```bash
/usr/local/cuda/bin/compute-sanitizer --tool memcheck --target-processes all \
  ./raytracer --path scenes/showcase.cfg --cli --renderer cuda \
  --width 64 --height 36 --frames 8 --output /tmp/sanitizer.ppm
```

CPU-only configuration:

```bash
cmake -S . -B /tmp/raytracer-cpu/build -DRAYTRACER_ENABLE_CUDA=OFF
cmake --build /tmp/raytracer-cpu/build -j"$(nproc)"
cd /tmp/raytracer-cpu && ./raytracer --list-renderers
```

GUI smoke test (requires a display or Xvfb):

```bash
./raytracer --path scenes/showcase.cfg --sfml --renderer cuda
```

Scene render scripts:

```bash
./scenes/render_hd.sh
RENDERER=cuda FRAMES=1000 ./scenes/render_hd.sh
./scenes/render_8k.sh
```

## Debugging and performance

- CPU vs CUDA: CUDA is typically orders of magnitude faster at high sample
  counts. Use the CPU renderer for debugging renderer logic and the CUDA renderer
  for final high-resolution renders.
- The CUDA renderer is compiled for compute capability `89` by default. A kernel
  image error on another GPU means `-DRAYTRACER_CUDA_ARCH=<cc>` must be used and
  the plugin rebuilt.
- 8K CUDA sessions use roughly 530 MB for accumulation and 130 MB for the output
  buffer, plus texture memory in GUI mode. 8K should be rendered with `--cli`.
- `Infinity Mirror` is the most expensive scene because of nested metallic
  reflections. Lower `FRAMES` or use a simpler scene for quick tests.
- PPM output is binary `P6`; use Pillow or ImageMagick to inspect or convert it.
- The render scripts produce 4K and 1080p previews so you do not need to open
  8K files directly.

## Contributing

1. Create a focused branch.
2. Keep changes consistent with the existing style and plugin boundaries.
3. Build with `-Werror` and test both CPU and CUDA when touching shared renderer
   code.
4. Update documentation for user-facing changes.
5. Run the project pre-commit hook if Docker is available:

   ```bash
   ./pre-commit.sh
   ```

## Further documentation

- [../README.md](../README.md) — project overview, build and usage.
- [cli.md](cli.md) — complete CLI reference.
- [scene-format.md](scene-format.md) — scene configuration reference.
- [renderers.md](renderers.md) — renderer plugin API and CUDA details.
- [../scenes/README.md](../scenes/README.md) — scene list and render scripts.
