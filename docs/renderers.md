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
    int frames;    // samples per pixel
    std::string outputPath;
    bool animation;
    int startFrame;
    int endFrame;
    double animationFps;
    float shutterAngle;
    int shutterSamples;
    float lensRadiusOverride;
    float focusDistanceOverride;
    bool resume;
    std::string scenePath;
    std::string rendererName;
    std::string videoPath;
    std::string videoCodec;
    int videoCrf;
    std::string videoPreset;
    std::string videoBitrate;
};

struct RenderContext
{
    std::shared_ptr<Scene> scene;
    std::shared_ptr<Camera> camera;
    std::shared_ptr<CameraAnimation> animation;
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
- OBJ vertex normals are interpolated with barycentric coordinates for smooth
  shading; geometric normals remain responsible for ray offsets and
  dielectrics.
- Material albedo, roughness, emission and normal maps are sampled bilinearly
  with repeat wrapping at the hit UV. UVs come from analytic primitive mappings
  or interpolated OBJ `vt` data. Normal maps currently use a shading-normal
  tangent frame rather than mesh tangents.
- Primitives include spheres, planes, cylinders, cones, axis-aligned boxes,
  disks/rings and OBJ meshes. Boxes and disks use the same instance-transform
  path as the other primitives.
- Prefab scatter expands deterministic instances at parse time and feeds the
  normal BVH/mesh instancing path.
- Equirectangular environment backgrounds are sampled on miss with a clamped
  V coordinate. Diffuse surfaces additionally use an importance-sampled
  luminance CDF and MIS for explicit environment lighting.
- OBJ `usemtl` groups resolve to per-triangle material overrides. Unmapped
  groups use the mesh's fallback material. `mtllib` materials are imported as
  Phong-style materials with optional `map_Kd` textures.
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
- Sphere area lights are sampled as solid-angle cone sources. A direction is
  sampled uniformly inside the sphere's subtended cone, the shadow ray targets
  the sphere entry point, and radiance is scaled by the cone's solid angle. The
  light loop uses `ILight::sample()` for a consistent direction, radiance and
  visibility query.
- In GUI mode, window events are polled while workers trace a frame. A camera
  event aborts the current frame and restarts with the new pose, so heavy CPU
  frames do not freeze the window.
- Writes binary `P6` PPM or 8-bit RGBA PNG through the shared image writer.
- CPU sampling uses the same per-pixel/sample PCG seed as CUDA, so renders are
  deterministic and the two renderers converge to the same image.
- In animation mode, interpolates a `Camera` for each frame and renders the
  sample count requested by `--samples`, writing one file per frame.

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
  Möller–Trumbore. Triangles carry per-corner normals; both the geometric and
  interpolated shading normal are propagated through the intersection chain.
- **Transforms:** `PrimitiveData::transform` is copied into `Cuda::Primitive`;
  rays are transformed into local space, and normals are transformed back with
  the inverse-transpose.
- **Depth of field:** `CameraData` carries the lens radius, focus distance and
  camera basis. The kernel consumes lens samples in the same order as the CPU
  and skips them entirely when the lens radius is zero.
- **Area lights:** sphere lights consume two random values per shading query,
  sample a point on the sphere, and trace a shadow ray. The CPU and CUDA loops
  must use the same light order and sampling order for parity.
- **Textures:** unique PPM/PNG/JPG/HDR textures are uploaded once as global
  `Vec3` texels with a `TextureRange` per texture. Materials store albedo,
  roughness, emission and normal indices and the device samples bilinearly with
  repeat wrapping.
- **Primitives:** spheres, planes, cylinders, cones, boxes, disks and meshes are
  intersected on device with the same local/instance transform path.
- **Scatter:** prefab scatter is expanded on the host during parsing, so the
  device sees ordinary mesh/primitive instances.
- **OBJ materials:** per-primitive group maps are uploaded as a flat `int`
  array with ranges. Triangle hits report a group and `intersectMesh` resolves
  it to a material override. Imported MTL materials use the same data path as
  scene-authored materials.
- **Environment maps:** the background texture shares the material texture
  upload path. `skyColor` samples it with clamped-V equirectangular mapping and
  falls back to the gradient when no environment is configured.
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
- **Output:** uses the shared image writer and supports stdout, PPM/PNG files,
  and unwritable-output errors like the CPU renderer. PPM output is binary `P6`,
  which keeps 8K images around 100 MB instead of several hundred MB of ASCII.
- **Animation:** builds the snapshot and `CudaRenderSession` once from the
  first frame, then reuses the scene, BVH, meshes, textures and lights for every
  later frame. `updateCamera()` replaces the camera and clears accumulation;
  `setSeed()` applies the deterministic per-frame seed. The session is never
  rebuilt for a camera-only animation, so long sequences avoid the upload cost.

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
re-tonemapped from the accumulated HDR buffer; CUDA uses matching device
bright-pass, Gaussian blur and composite bloom kernels. The two paths share the
same bloom kernel weights and parameterization. CUDA uses a device post-process
pass over its accumulation buffer.

## Output pipeline

Both renderers share the same output pipeline:

1. linear radiance,
2. exposure multiplier,
3. tonemapping: `none`, `reinhard`, or `aces`,
4. sRGB transfer,
5. 8-bit PPM, PNG or video output.

Video frames use the same display-referred pixels, streamed as RGBA to FFmpeg
and converted to BT.709 `yuv420p`. MP4/MOV/MKV use H.264; WebM uses VP9.

The defaults are exposure `1.0` and tonemapping `none`. Use `--tonemap aces`
for a softer highlight roll-off and `--exposure` to adjust the overall level.
Missed rays sample the optional scene `background` gradient instead of returning
black.

## Animation

Both renderers play the same `CameraAnimation` timeline and share the per-frame
seed formula `seed + frame * 0x9e3779b9u`.

- The CPU renderer creates a fresh `Renderer` per subframe with the interpolated
  camera and reads its linear HDR average. This reuses the immutable scene graph
  and avoids rebuilding geometry; allocation cost is small relative to tracing.
- The CUDA renderer keeps one `CudaRenderSession` alive and updates only
  `CameraData` and the seed between subframes. Device memory for the BVH,
  meshes and textures is allocated once.
- Motion blur accumulates subframes in linear HDR and post-processes once.
  CUDA uses a lazily allocated device shutter buffer and an accumulation
  kernel; shutter-off sessions keep their previous memory use.
- Both write through `formatFramePath` and `writeImageFileAtomic`, so `.ppm`
  and `.png` sequences use identical names and numbering, and interrupted
  writes never leave partial final frames.
- `--resume` skips frames whose final output already exists. The CUDA session
  is initialized from the first frame that will actually render.
- Named prefab-instance transforms and light values can also animate. CPU
  rebuilds the BVH only when instance transforms change; CUDA reuses the session
  for light-only animation and rebuilds the snapshot/session per frame when
  instances move. Camera-only sequences keep the original fast path.
- Motion blur is camera-only; object and light values are constant within a
  shutter interval.
- A JSON sidecar records the resolved settings, video settings, scene-animation
  targets and per-frame camera poses; both renderers produce the same schema.
- `--video` streams final frames to the same `VideoWriter` in both renderers.
  Video-only mode writes no frame files; `--output` or `--keep-frames` enables
  them. Video output uses an FFmpeg subprocess and requires the `ffmpeg`
  executable at render time.
- Animation is CLI-only. GUI renderers reject animation flags so interactive
  camera control cannot fight the timeline.
- CPU/CUDA sequences match within normal Monte Carlo noise when the sample
  count and seed are identical. Camera-only animation does not change scene
  ordering; scene animation keeps the same primitive and light order on both
  renderers, so parity behavior is unchanged from still rendering.

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

