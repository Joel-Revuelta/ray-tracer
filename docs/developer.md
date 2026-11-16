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
- C++20 compiler (tested with GCC 15.2 and Clang 21.1)
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
# Keep the executable and plugins inside the build tree (CI, multiple builds)
cmake -S . -B build -DRAYTRACER_IN_SOURCE_OUTPUT=OFF

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

By default the executable is written to the repository root, plugins to
`plugins/`, and `libraytracer_core.so` stays in `build/`. Run the executable from
the repository root so that `./plugins/` resolves. With
`-DRAYTRACER_IN_SOURCE_OUTPUT=OFF` the executable lands in `build/` and plugins
in `build/plugins/`; plugin lookup starts next to the executable, so the binary
can be run from anywhere.

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
| `src/render/` | Renderer registry, animation timeline, PPM/PNG/video writers, camera controller, scene snapshot. |
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
2. Resolves the optional camera animation, frame range and FPS.
3. Looks up the requested renderer in `RendererRegistry`.
4. Creates the renderer plugin through `Factory`.
5. Checks `IRenderer::isAvailable()`.
6. Builds a `RenderContext` with the scene, camera, optional animation, optional
   GUI and options.
7. Calls `IRenderer::run()`.

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

`Triangle` stores three positions, three vertex normals and three UV pairs. The
OBJ loader fills them from `vn`/`vt` data and falls back to the face normal and
zero UVs when missing. `HitPayload` carries both a geometric and a shading
normal: `setHitNormal()` also seeds the shading normal, and smooth meshes
override it with `setShadingNormal()`. `InstancePrimitive` transforms both
normals with the inverse-transpose. Renderers use the shading normal for
diffuse/metallic sampling and lighting, and the geometric normal for ray
offsets, termination checks and dielectric reflection/refraction. CUDA
propagates both normals through the intersection chain (`bestNormal`,
`bestShadingNormal`).

### Textures and UVs

`include/render/Texture.hpp` provides an RGB `Texture` with bilinear,
repeat-wrapped sampling. PPM uses the in-tree parser; PNG/JPG and Radiance HDR
use the vendored single-header `stb_image`. `loadTextureFile` accepts a
`TextureColorSpace`: albedo and emission maps are decoded from sRGB, while
roughness and normal maps stay linear. HDR values are always linear. The parser
caches textures by resolved path and colour space, then attaches them through
`IMaterial::setTexture`, `setRoughnessTexture`, `setEmissionTexture` and
`setNormalTexture`; `AMaterial` stores the shared pointers.

Primitives set local UVs in `HitPayload`:

- spheres use longitude/latitude,
- planes use local X/Z,
- cylinders use angle/normalized height,
- cones use angle/normalized apex-to-base height,
- meshes interpolate OBJ `vt` values with the triangle barycentrics.

`InstancePrimitive` leaves UVs unchanged because they are already local.

Both renderers compute one `surfaceColor = diffuseColor * texture.sample(u, v)`
per hit and use it for direct lighting, metallic sampling and diffuse
throughput. Roughness maps multiply the scalar roughness; emission maps
multiply the emission colour. A missing texture keeps the old constant path.

Normal maps decode RGB to `[-1, 1]` and perturb the shading normal. The current
tangent frame is derived deterministically from the shading normal rather than
from mesh/primitive UV tangents. This keeps CPU/CUDA parity simple, but rotating
a normal map around the UV axis is a future improvement.

For CUDA, `SceneSnapshot` deduplicates all four texture channels and
`CudaRenderSession` uploads a compact global `Vec3` texel buffer plus one
`TextureRange` per texture; `Cuda::Material` stores four indices and the device
samples with the same bilinear repeat mapping.
The procedural hero-scene textures and meshes are generated by
`tests/generate_scene_assets.py`.

### Bloom and post-processing

`PostProcessSettings` can optionally enable a bright-pass, separable Gaussian
bloom before exposure and tonemapping. CPU and CUDA share the same Gaussian
kernel generated by `bloomKernel()`; the CUDA path uses bright-pass, horizontal
blur, vertical blur and composite kernels. Bloom buffers are allocated only
when bloom is enabled.

### Environment maps

`Background` optionally stores a shared environment `Texture`. When present,
`Background::sample()` maps the miss direction to equirectangular coordinates:

```text
u = 0.5 + atan2(z, x) / (2π)
v = acos(clamp(y, -1, 1)) / π
```

U wraps and V is clamped, using `Texture::sampleClamped()` so the poles do not
wrap into each other. The environment replaces the gradient background and is
also returned by every path-traced miss, so it provides reflections and
indirect illumination without a separate light-sampling path.

`buildSceneSnapshot()` registers the background texture in the same
deduplicated texture list as material albedo maps and stores its index in
`Background::environmentTextureIndex`. CUDA uploads that list once;
`skyColor()` samples the environment when the index is valid, otherwise it
keeps the original gradient calculation. Gradient-only scenes therefore render
identically.

`buildEnvironmentDistribution()` builds a luminance CDF over the equirect
texels, including the `sin(theta)` solid-angle factor. Diffuse hits use this
distribution for explicit environment lighting and combine it with
cosine-weighted BSDF sampling through MIS. CUDA uploads the conditional and
marginal CDFs; both renderers use the same sampling and PDF formulas.
Environment maps support PPM, PNG, JPG and Radiance RGBE `.hdr`; HDR values
above `1.0` are preserved.

### OBJ material groups

The OBJ loader records `usemtl` names in `MeshAsset::materialNames` and stores
a `materialGroup` index on every triangle. `-1` means the face had no preceding
`usemtl`.

Each mesh scene entry maps group names to scene material indices through
`materialGroups`. `MeshPrimitive` resolves that map once into a per-instance
`vector<int>` aligned with the asset's group list; unmatched groups stay `-1`
and use the primitive's fallback material. The resolved map is shared with the
CUDA snapshot and uploaded as a flat `int` array with per-primitive ranges.

`HitPayload` carries a material override (`-1` by default). Mesh triangles set
it when a group resolves, and `InstancePrimitive` forwards it. The CPU renderer
uses the override when present, otherwise the primitive material.

CUDA propagates `bestMaterialIndex` through the intersection chain.
`intersectTriangle` reports the hit triangle's group, and `intersectMesh`
resolves it through the primitive's uploaded range. `traceSample` then selects
the override or the primitive fallback.

`SceneBuilder::addScene` shifts both primitive material indices and mesh group
maps when nested scenes are merged, so nested group indices remain valid. It
also accepts an optional `TransformData`:

- primitives are cloned, material-adjusted, then wrapped in
  `InstancePrimitive` when the transform is non-identity;
- lights are cloned and receive `ILight::applyTransform`;
- `getMaterialCount()` exposes the scene's material total so the parser keeps
  `_materialCount` in sync after composition.

`ILight::applyTransform` defaults to a no-op. Point lights transform their
position, directional lights rotate and normalise their direction, and sphere
area lights transform their centre and scale the radius by
`transformScaleFactor()` (the largest basis-column length). Ambient lights
ignore transforms.

Prefabs are parsed into a name → `Scene` map before top-level primitives.
`primitives.instances` looks up the prefab, parses an optional transform, and
calls `addScene`. Prefab parsing sets a parser flag so any `instances` block
inside a prefab raises an error, which keeps recursion impossible. Unknown
prefab names are rejected.

### MTL import

`MeshLoader` records `mtllib` paths in `MeshAsset::materialLibraries`, resolved
relative to the OBJ. `loadMtl()` parses `newmtl`, `Ka`, `Kd`, `Ks`, `Ke`, `Ns`,
`d`/`Tr` and `map_Kd`; `map_Kd` resolves relative to the MTL file.

The parser caches parsed MTL files and, per scene, appends unseen MTL materials
as `PhongMaterial` instances. Mapping is approximate:

- `Kd` becomes the diffuse colour.
- `Ka` luminance becomes the ambient reflection coefficient.
- `Ks` luminance becomes the specular reflection coefficient.
- `Ns` becomes roughness via `sqrt(2 / (Ns + 2))`.
- `map_Kd` is attached through `AMaterial::setTexture`.

An automatic `usemtl → material index` map is built from MTL names. Explicit
`materialGroups` entries are applied first and always win. Unmatched groups use
the mesh fallback. The per-scene MTL index map is saved and restored around
nested scene parsing, matching the existing material-count guard.

Emission (`Ke`) and opacity (`d`/`Tr`) are parsed but not yet applied to
rendering.

### Renderer interface

Renderers implement `RayTracer::IRenderer` from
`include/plugins/renderers/IRenderer.hpp`:

```cpp
struct RenderOptions
{
    int width;
    int height;
    int frames;    // samples per pixel
    int threads;   // CPU only, 0 = automatic
    int device;    // CUDA only
    uint32_t seed;
    std::string outputPath;
    PostProcessSettings postProcess;
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
    std::shared_ptr<SceneAnimation> sceneAnimation;
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
- `run()` performs the render and writes a PPM or PNG when requested.
- The CPU renderer walks the plugin-backed scene graph directly.
- The CUDA renderer uses `buildSceneSnapshot(scene, camera)` to obtain a flat,
  device-friendly representation.
- When `options.animation` is set, `run()` iterates the inclusive frame range
  and writes one numbered file per frame instead of a single image.

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

### Camera animation

`render/Animation.hpp` defines `CameraKeyframe` and `CameraAnimation`. The
parser resolves omitted keyframe fields and constructs one timeline per scene.
`CameraAnimation::cameraAt(time)`:

1. clamps the time to the timeline,
2. finds the surrounding keyframe pair,
3. interpolates position and look-at with Catmull-Rom (endpoint keyframes are
   duplicated as phantom neighbours),
4. interpolates FOV, lens radius and focus distance linearly,
5. returns a new `Camera` with the requested pose.

Frame helpers keep sequence rendering consistent:

```cpp
const int frameCount = animation->getFrameCount(fps);
const double time = animation->timeAtFrame(frame, fps);
```

`timeAtFrame` returns `frame / fps` clamped to the last keyframe, and
`getFrameCount` returns `ceil(duration * fps)` (at least one frame).

`shutterSampleTime(frameTime, fps, shutter, sample)` returns the stratified
subframe times for camera motion blur. The interval is centered on the frame
time and spans `shutter.angle / 360 / fps` seconds; `cameraAt` clamps times
outside the timeline.

Sequence rendering keeps still rendering untouched:

- **CPU** constructs a `Renderer` per subframe and calls `renderHDR()` to get
  the linear HDR average without per-subframe bloom or file output. Subframes
  are averaged into one frame buffer, post-processed once, and written
  atomically.
- **CUDA** builds the `SceneSnapshot` and `CudaRenderSession` once from the
  first rendered frame. Every rendered subframe calls
  `updateCamera(makeCameraData(camera))` (which clears accumulation) and
  `setSeed`. With motion blur enabled, `beginShutter` allocates a device
  accumulation buffer and `accumulateShutter` adds each subframe's samples;
  `refreshShutterOutput` runs the existing bloom/post-process pass over the
  accumulated buffer. The extra buffer is allocated lazily, so shutter-off
  sessions keep their previous memory use.
- The per-frame seed is `seed + frame * 0x9e3779b9u`; additional shutter
  subframes add `subframe * 0x85ebca6bu`. Sequences are deterministic and
  independent of thread count.
- `--resume` skips frames whose final output already exists. The CUDA session
  is initialized from the first frame that will actually be rendered.
- `formatFramePath` accepts one `d`/`i`/`u` printf conversion, escapes `%%`,
  and appends a padded `-NNNN` suffix when the pattern has no conversion.
- `writeImageFileAtomic` writes PPM/PNG through a temporary file and renames it
  into place. `writeImageFile` remains the non-atomic primitive.
- `writeSequenceMetadata` writes the JSON sidecar described in
  [cli.md](cli.md#animation). `sequenceMetadataPath` derives its name from the
  output pattern.

### Scene animation

`render/SceneAnimation` adds named prefab-instance transform tracks and light
tracks alongside the camera timeline.

- Prefab instances and lights register optional IDs in `Scene`. Duplicate IDs
  are rejected.
- `SceneBuilder::addScene` wraps every primitive of a named instance in
  `InstancePrimitive`, even for an identity transform, and registers the group.
- `InstancePrimitive` stores a `TransformPose` (translation, Euler rotation,
  scale) alongside the derived matrix so omitted track channels can inherit the
  base values.
- `Quaternion` conversion and slerp live in `scene/Transform`; endpoint
  keyframes use the Euler path directly so frame `0` remains byte-identical to
  the still render.
- `SceneAnimation::apply` updates instance transforms and light values. It
  reports whether instance transforms changed.
- CPU applies the timeline once per frame and rebuilds the scene BVH only when
  instances moved.
- CUDA has three paths:
  - camera-only: reuse the existing `CudaRenderSession`;
  - light-only: reuse the session and upload an updated light buffer through
    `updateLights`;
  - instance transforms: rebuild the `SceneSnapshot` and session per frame for
    v1. Dynamic BVH refitting is future work.
- Motion blur remains camera-only: object and light values are constant across
  shutter subframes.
- Still renders never call `SceneAnimation::apply`.

### Video output

`render/VideoWriter` streams animation frames to the `ffmpeg` executable. It
does not link `libav*`, so video support adds no link-time dependency and works
with whatever encoders the installed FFmpeg provides.

- `findFFmpeg()` resolves `RAYTRACER_FFMPEG` or searches `PATH`.
- `validateVideoSettings()` checks the container/codec pairing, CRF range,
  preset, bitrate, frame rate and dimensions before rendering starts.
- `VideoWriter` creates a pipe, forks and execs FFmpeg without a shell, and
  writes raw RGBA frames to its stdin.
- The encoder writes to a temporary path such as `orrery.tmp.mp4`; `finish()`
  waits for FFmpeg and renames the result. A destructor or failed render removes
  the temporary file.
- `SIGPIPE` is ignored so a dead encoder becomes an error with FFmpeg's exit
  status rather than terminating the renderer.
- Odd dimensions are padded to even for `yuv420p`; frames are converted with a
  BT.709 matrix and tagged accordingly.
- MP4/MOV/MKV use `libx264`; WebM uses `libvpx-vp9`. `--video-bitrate` switches
  from CRF to bitrate mode.

Both animation loops create one writer before rendering, stream each final frame
after motion blur and post-processing, and call `finish()` before writing
metadata. Video-only mode leaves `outputPath` empty; frame writing is opt-in via
`--output` or `--keep-frames`. `--resume` is rejected with video output because
a partially encoded stream cannot be resumed reliably.

### Lights and area sampling

`ILight::sample(payload, scene, seed)` returns a `LightSample` containing a
direction, attenuated radiance and a validity flag. Its default implementation
calls `isPointInShadow()`, `getLightDirection()` and `getRadiance()`, so
ambient, directional and point lights keep their previous behavior and consume
no random values.

`SphereLight` overrides `sample()`:

- two random values select a direction uniformly in the cone subtended by the
  sphere,
- samples below the shading surface are rejected,
- the shadow ray targets the sphere's entry point along that direction,
- radiance is scaled by the cone's solid angle and uses the same
  constant/linear/quadratic attenuation as point lights,
- `intensity` is therefore radiance-like; the area-light scenes use larger
  values than point-light scenes.

The CPU and CUDA light loops must stay aligned: same light order and exactly
two random values per sphere-light sample. The shadow-ray offset uses the
geometric normal, while the N·L visibility test uses the shading normal.
Soft-shadow noise is expected; higher sample counts are required for clean
penumbrae.

## Adding components

### Primitive

1. Add `include/plugins/primitives/MyPrimitive.hpp` and
   `src/plugins/primitives/MyPrimitive.cpp`.
2. Derive from `IPrimitive` and implement:
   - `hit(const Ray &, HitPayload &, size_t)`
   - `getData(PrimitiveData &)` for GPU/flat renderers
   - `getMaterialIndex()` / `setMaterialIndex()`
   - `doTransformation()` (or intentionally ignore it)
   - `clone()` so nested-scene merging can copy the primitive safely;
     composition rejects a null clone rather than mutating a shared instance
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

Run the full suite or a labeled subset:

```bash
ctest --test-dir build --output-on-failure
ctest --test-dir build -L animation
ctest --test-dir build -L negative
ctest --test-dir build -L parity
```

Labels are `unit`, `smoke`, `scene`, `animation`, `output`, `negative`, `video`,
`parity`, `parser`, `golden` and `benchmark`. The parity label includes the
CPU/CUDA comparison; it skips with status `77` when no CUDA device is available.
The video test also skips with `77` when FFmpeg is missing.

```bash
ctest --test-dir build -L golden
ctest --test-dir build -L benchmark
UPDATE_GOLDEN=1 bash tests/golden_images.sh ./raytracer . build
bash tests/malformed_scene_corpus.sh ./raytracer . build
bash tests/benchmark_report.sh ./raytracer . build
```

CI runs headless GCC and Clang Release builds, an ASan/UBSan Debug build, and an
optional self-hosted CUDA job enabled with the repository variable
`RAYTRACER_CUDA_CI=true`. The CPU jobs install FFmpeg so the `video` tests run;
the script skips with status `77` when FFmpeg is missing. Golden-image,
malformed-corpus and benchmark tests run with the normal suite, and the
benchmark CSV is uploaded as a CI artifact. GUI builds remain a manual
verification step because SFML 3 is not packaged on the default runner.

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

CPU/CUDA parity (still and animated frame):

```bash
bash tests/cpu_cuda_parity.sh ./raytracer . build
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

Soft-shadow check:

```bash
./raytracer --path scenes/soft_shadows.cfg --cli --renderer cpu \
  --width 320 --height 180 --frames 32 --output /tmp/soft.ppm
bash tests/deterministic_soft_shadow.sh ./raytracer . build
```

Animation and PNG output:

```bash
./raytracer --path scenes/brass_orrery.cfg --cli --renderer cpu \
  --width 320 --height 180 --samples 8 --animation \
  --start-frame 0 --end-frame 11 --output '/tmp/orrery-%04d.png'
bash tests/deterministic_animation.sh ./raytracer . build
bash tests/png_output.sh ./raytracer . build
```

Motion blur, metadata and resume:

```bash
./raytracer --path scenes/brass_orrery.cfg --cli --renderer cpu \
  --width 320 --height 180 --samples 8 --animation \
  --shutter-angle 180 --shutter-samples 4 \
  --start-frame 0 --end-frame 11 --output '/tmp/orrery-%04d.png'
bash tests/deterministic_motion_blur.sh ./raytracer . build
./raytracer --path scenes/brass_orrery.cfg --cli --animation \
  --output '/tmp/orrery-%04d.png' --validate
./raytracer --path scenes/brass_orrery.cfg --cli --renderer cpu \
  --width 320 --height 180 --samples 8 --animation --resume \
  --output '/tmp/orrery-%04d.png'
```

Scene animation:

```bash
./raytracer --path tests/scene_animation.cfg --cli --renderer cpu \
  --width 320 --height 180 --samples 8 --animation \
  --start-frame 0 --end-frame 23 --output '/tmp/scene-%04d.ppm'
bash tests/deterministic_scene_animation.sh ./raytracer . build
```

Video output:

```bash
./raytracer --path scenes/brass_orrery.cfg --cli --renderer cpu \
  --width 320 --height 180 --samples 8 --animation \
  --video /tmp/orrery.mp4
ffprobe -v error -count_frames -select_streams v:0 \
  -show_entries stream=width,height,r_frame_rate,nb_read_frames \
  -of default=nw=1 /tmp/orrery.mp4
bash tests/video_output.sh ./raytracer . build
```

`tests/video_output.sh` skips with status `77` when FFmpeg is unavailable.
Install it with your package manager or set `RAYTRACER_FFMPEG` to a custom
binary.

Texture check:

```bash
./raytracer --path scenes/textures.cfg --cli --renderer cpu \
  --width 320 --height 180 --frames 16 --output /tmp/textures.ppm
bash tests/deterministic_texture.sh ./raytracer . build
```

Material maps, boxes/disks and scatter:

```bash
./raytracer --path scenes/material_maps.cfg --cli --renderer cpu \
  --width 320 --height 180 --samples 32 --output /tmp/maps.png
./raytracer --path scenes/primitives_extra.cfg --cli --renderer cpu \
  --width 320 --height 180 --samples 32 --output /tmp/extra.png
./raytracer --path scenes/scatter_field.cfg --cli --renderer cuda \
  --width 320 --height 180 --samples 32 --output /tmp/scatter.png
bash tests/golden_images.sh ./raytracer . build
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
- PPM output is binary `P6`; PNG output is 8-bit RGBA. Both store
  display-referred values after exposure, tonemapping and the sRGB transfer.
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
