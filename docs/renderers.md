# Renderer plugins

Renderers are runtime plugins. The executable only parses arguments, builds the
scene and selects a renderer; the rendering implementation lives in a shared
library under `plugins/`.

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
- **Architecture:** currently compiled for `sm_89` (RTX 4070 Laptop / Ada).
  Change `CUDA_ARCHITECTURES` in `CMakeLists.txt` for other GPUs.
- **Runtime:** the plugin links `CUDA::cudart_static`, so it does not require a
  separate CUDA runtime library path at run time.
- **Availability:** `isAvailable()` returns false with the driver error when
  `cudaGetDeviceCount()` fails or reports no devices.
- **GUI:** the CUDA plugin supports `--sfml` and the shared `CameraController`
  controls. Rendering is split into small sample batches; the window is updated
  and events are pumped between batches, so the window stays responsive and
  camera movement restarts the render promptly.
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

The CUDA kernel uses a seeded PCG sampler. CPU and CUDA renders share the same
scene, materials, lights, intersections and bounce rules; at equal sample counts
their averages and large-scale structure match closely, with independent noise
patterns.

Tested with CUDA 13.4, GCC 15.2, NVIDIA driver 595.99.02 and an RTX 4070 Laptop
(`sm_89`).

