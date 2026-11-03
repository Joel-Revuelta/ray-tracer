# Renderer plugins

Renderers are runtime plugins. The executable only parses arguments, builds the
scene and selects a renderer; the rendering implementation lives in a shared
library under `plugins/`.

See [../README.md](../README.md) for build and usage instructions,
[cli.md](cli.md) for renderer selection from the command line, and
[scene-format.md](scene-format.md) for the scene configuration format.

## Selecting a renderer

```bash
./raytracer --list-renderers
./raytracer --path scenes/showcase.cfg --cli --renderer cpu
./raytracer --path scenes/showcase.cfg --cli --renderer cuda
```

The default is `cpu`. Renderer names are case-insensitive. `--list-renderers`
loads each renderer plugin and reports whether it is available, including the
reason when it is not.

## Architecture

- `raytracer_core` is a shared library containing the camera, scene graph,
  parser, plugin loader, renderer interface and shared render helpers.
- Every plugin links against `raytracer_core`, so objects such as `Scene`,
  `Camera` and `IGUI` have one definition and one set of RTTI symbols across the
  executable and all plugins.
- A renderer implements `RayTracer::IRenderer` from
  `include/plugins/renderers/IRenderer.hpp`.
- The plugin exports:

  ```cpp
  extern "C" std::shared_ptr<RayTracer::IRenderer> create(void);
  ```

- `RendererRegistry.cpp` maps the user-facing name (`cpu`, `cuda`) to a Factory
  key. `Factory` is the generic plugin loader and maps that key to the shared
  library name.

## Renderer interface

```cpp
struct RenderOptions
{
    int width;
    int height;
    int frames;
    std::string outputPath;
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

`isAvailable()` is used by `--list-renderers` and by `--renderer` before
rendering. It should return `false` with a human-readable reason when the
required device or runtime is missing.

## Adding a renderer

1. Add a directory under `src/plugins/renderers/<Name>/`.
2. Implement `IRenderer` and export `create()`.
3. Add a CMake target linking `raytracer_core`, and include it in the
   `LIBRARY_OUTPUT_DIRECTORY` list so it is written to `plugins/`.
4. Add an entry in `src/render/RendererRegistry.cpp` and a library mapping in
   `src/factory/Factory.cpp`.
5. Add the renderer to the documentation and help text.

## Flat scene data

The CPU renderer traverses the plugin-backed scene graph directly. Renderers
that need a flat memory layout (for example, GPU renderers) can call:

```cpp
SceneSnapshot snapshot = buildSceneSnapshot(*scene, *camera);
```

`SceneSnapshot` contains POD-friendly arrays of primitives, materials, lights
and camera data. Primitives and lights expose `getData()` for this purpose, so
the renderer does not need to `dynamic_cast` concrete plugin classes.

## CPU renderer

The CPU renderer lives in `src/plugins/renderers/CPU/` and is the default.

- Multithreaded: the image is split into horizontal bands and rendered by a
  persistent worker pool, one worker per band.
- Hot vector math is header-inlined, and the renderer uses a binned-SAH BVH for
  bounded primitives; planes and other unbounded primitives are still tested
  linearly. Traversal visits the nearer child first and can skip the far child.
- Meshes use a second-level local triangle BVH. The scene BVH finds the mesh
  instance, then `MeshPrimitive` traverses the shared asset's triangle BVH.
- Walks the plugin-backed scene graph directly through `Scene`/`IPrimitive`.
- Uses the same 15-bounce path tracing, material and light logic as the CUDA
  renderer. Path throughput is attenuated by diffuse/metallic surface colour,
  diffuse bounces use cosine-weighted hemisphere sampling, metallic surfaces
  use GGX/Smith roughness sampling with Schlick Fresnel, and direct lighting
  includes Blinn-Phong specular highlights.
- Supports `--gui sfml` and `--gui opengl`, plus the shared `CameraController`
  controls, including live lens-radius and focus-distance adjustment.
- Camera rays support a world-space lens disk and focus plane for depth of
  field; a zero lens radius keeps the pinhole path.
- In GUI mode, window events are polled while workers trace a frame. A camera
  event aborts the current frame and restarts with the new pose, so heavy CPU
  frames do not freeze the window.
- Writes binary `P6` PPM through the shared PPM helper.
- CPU sampling uses the same per-pixel/sample PCG seed as CUDA, so renders are
  deterministic and the two renderers converge to the same image.

The CPU renderer is the reference implementation for visual behavior and is
useful for debugging. Use CUDA for high-resolution or high-sample final renders.

## CUDA renderer

The CUDA renderer mirrors the CPU lighting, intersection and bounce rules and
uses the same `SceneSnapshot` data path.

- **Sources:**
  - `include/plugins/renderers/CUDA/CUDARenderer.hpp`
  - `include/plugins/renderers/CUDA/CudaScene.hpp`
  - `include/plugins/renderers/CUDA/CudaKernels.hpp`
  - `src/plugins/renderers/CUDA/CUDARenderer.cpp`
  - `src/plugins/renderers/CUDA/Kernels.cu`
- **Build:** CMake detects the toolkit with `find_package(CUDAToolkit QUIET)`.
  The target is built when CUDA is available and can be disabled with
  `-DRAYTRACER_ENABLE_CUDA=OFF`.
- **Target:** `raytracer_renderer_cuda`, written to
  `plugins/raytracer_renderer_cuda.so`.
- **Acceleration:** builds the same shared binned-SAH BVH as the CPU renderer,
  uploads it to the device, and traverses it near-first; planes and other
  unbounded primitives are tested linearly.
- **Meshes:** unique assets are uploaded once as global triangle/BVH/index
  buffers with a `MeshRange` per asset. Instances reference a range and reuse
  the same geometry. Device traversal is scene BVH → mesh BVH →
  Möller–Trumbore.
- **Transforms:** `PrimitiveData::transform` is copied into `Cuda::Primitive`;
  rays are transformed into local space, and normals are transformed back with
  the inverse-transpose.
- **Depth of field:** `CameraData` carries the lens radius, focus distance and
  camera basis. The kernel consumes lens samples in the same order as the CPU
  and skips them entirely when the lens radius is zero.
- **Architecture:** compiled for `89` by default (RTX 4070 Laptop / Ada). Set
  `-DRAYTRACER_CUDA_ARCH=86` (or `86;89`) for other GPUs.
- **Runtime:** the plugin links `CUDA::cudart_static`, so it does not require a
  separate CUDA runtime library path at run time.
- **Availability:** `isAvailable()` returns false with the driver error when
  `cudaGetDeviceCount()` fails or reports no devices, and when the device
  compute capability is below the architecture selected at build time.
- **GUI:** the CUDA plugin supports `--gui sfml` and `--gui opengl` with the
  shared `CameraController` controls (`WASD` movement, `Q`/`E` vertical, arrows
  or `I`/`J`/`K`/`L` look, `+`/`-` zoom, `R` reset, `X`/`C` locks). Rendering
  uses one-sample batches,
  but the host polls window events continuously while each GPU kernel is in
  flight. Camera movement is recorded during the kernel and the render restarts
  as soon as that sample completes, so heavy scenes do not freeze the window.
  Progressive texture uploads are capped at roughly 100 updates for large
  sample counts. Frame readback uses registered host memory when available.
- **Output:** uses the shared PPM helper and supports stdout, files, and
  unwritable-output errors like the CPU renderer. PPM output is binary `P6`,
  which keeps 8K images around 100 MB instead of several hundred MB of ASCII.

Run it with:

```bash
./raytracer --path scenes/showcase.cfg --cli --renderer cuda --frames 140 \
  --width 1920 --height 1080 --output render.ppm

RENDERER=cuda FRAMES=140 ./scenes/render_hd.sh
```

On this machine, a `1920x1080`, 140-frame Chrome Singularity render takes about
two seconds with CUDA, versus roughly ten minutes with the CPU renderer. A
1000-frame CUDA render of the same scene takes about three seconds, so high
sample counts are practical for final stills.

The 8K ultra scenes (`hypernova_cathedral`, `infinity_mirror`,
`crystal_superbloom`, `solar_eclipse`) were rendered at `7680x4320`, 2048 samples
per pixel on the same machine:

| Scene | CUDA time |
| --- | ---: |
| Hypernova Cathedral | 4m 29s |
| Infinity Mirror | 24m 02s |
| Crystal Superbloom | 4m 22s |
| Solar Eclipse | 3m 30s |

Infinity Mirror is the most expensive because of its nested metallic
reflections.

The CUDA kernel uses a seeded PCG sampler. CPU and CUDA renders share the same
scene, materials, lights, intersections, bounce rules and per-pixel/sample
seeds; at equal sample counts their images converge closely.

Tested with CUDA 13.4, GCC 15.2, NVIDIA driver 595.99.02 and an RTX 4070 Laptop
(`sm_89`).

## GUI backends

Both renderers use the same `IGUI` interface, so CPU and CUDA work with either
backend.

- `--gui sfml` / `--sfml`: SFML 3 window; resizing stretches the image.
- `--gui opengl`: GLFW window with an OpenGL 3.3 core context; resizing preserves
  the aspect ratio with letterboxing.

The OpenGL backend requires GLFW 3.3+, GLEW and an X11/GLX display. It is
disabled automatically when those dependencies are unavailable; `--gui opengl`
then reports the missing plugin.

Both GUIs support live statistics in the window title, `Space` pause/resume,
`P` screenshot saving, and `[`/`]` exposure changes. CPU screenshots are
re-tonemapped from the accumulated HDR buffer; CUDA uses a device post-process
pass over its accumulation buffer.

## Output pipeline

Both renderers share the same output pipeline:

1. linear radiance,
2. exposure multiplier,
3. tonemapping: `none`, `reinhard`, or `aces`,
4. sRGB transfer,
5. 8-bit PPM output.

The defaults are exposure `1.0` and tonemapping `none`. Use `--tonemap aces`
for a softer highlight roll-off and `--exposure` to adjust the overall level.
Missed rays sample the optional scene `background` gradient instead of returning
black.

## Troubleshooting

| Symptom | Resolution |
| --- | --- |
| `cannot open shared object file: ./plugins/...` | Run from the repository root, install the bundle, or set `RAYTRACER_PLUGIN_DIR` to the plugin directory. |
| `Renderer 'cuda' is unavailable` | CUDA was not built, the driver is missing, or no device was found. Check `--list-renderers`. |
| CUDA kernel image error | The plugin is compiled for a newer GPU than this machine; reconfigure with `-DRAYTRACER_CUDA_ARCH=<cc>` and rebuild. |
| GUI window appears unresponsive | The CUDA renderer polls events while the GPU kernel is running. Use `--cli` for 8K; if the problem persists, check the selected GUI backend and its display dependencies. |
| PPM will not open | Output is binary `P6`; use Pillow or ImageMagick. |
| 8K render is slow | Infinity Mirror is reflection-heavy; lower `FRAMES` or render a simpler scene for quick tests. |

## Further reading

- [../README.md](../README.md) — project overview and quick start.
- [cli.md](cli.md) — complete command-line reference.
- [scene-format.md](scene-format.md) — scene configuration reference.
- [developer.md](developer.md) — repository architecture and contribution guide.

