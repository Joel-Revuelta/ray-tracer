# AGENTS.md

## Build and run

- Default configure/build writes `./raytracer` and generated plugins to `./plugins/`; run `./raytracer` from the repository root. `libraytracer_core.so` stays in `build/`.
- Use `-DRAYTRACER_IN_SOURCE_OUTPUT=OFF` for isolated/CI builds; then run `./build/raytracer` and plugins live in `build/plugins/`.
- CMake defaults to Release. CUDA is optional and auto-detected; default `RAYTRACER_CUDA_ARCH=89`.
- SFML, GLFW/OpenGL, CUDA and FFmpeg are optional. Which plugins and tests exist depends on installed packages; use `ctest -N` for the real test set.

```bash
cmake -S . -B build
cmake --build build -j"$(nproc)"
ctest --test-dir build --output-on-failure
```

CPU-only build:

```bash
cmake -S . -B build -DRAYTRACER_ENABLE_CUDA=OFF
```

## Tests

- Single test: `ctest --test-dir build -R renderer_regressions --output-on-failure`
- Useful labels: `ctest --test-dir build -L animation`, `-L parity`, `-L negative`, `-L golden`, `-L parser`, `-L smoke`, `-L video`.
- Parity tests skip with status `77` when no CUDA device is available; video tests skip with `77` when FFmpeg is missing.
- GUI smoke tests are added only when `xvfb-run` and the corresponding GUI plugin are available; they also compare the GUI output byte-for-byte with a deterministic CLI render.
- Golden references are in `tests/golden/`. Refresh only intentional visual changes with:

```bash
UPDATE_GOLDEN=1 bash tests/golden_images.sh ./raytracer . build
```

Review the resulting PPM changes before keeping them. The normal golden test is `ctest --test-dir build -L golden`.

- Parser robustness: `bash tests/parser_fuzz_smoke.sh ./raytracer . build` and `bash tests/malformed_scene_corpus.sh ./raytracer . build`.
- Package/consumer check: `bash tests/package_consumer.sh . build /tmp/raytracer-install` (requires a built tree).

## CLI and scene gotchas

- `--samples` (and the legacy `--frames` alias) is samples per pixel, not an animation frame count. Animation ranges use `--start-frame`/`--end-frame`.
- `--validate` parses a scene and prints resolved settings without rendering; use it for quick parser checks.
- Output format is chosen by extension: only `.ppm` and `.png` are accepted; other extensions are rejected.
- In scene files, camera `rotation` is the look-at point, not Euler angles.

## Architecture

- `raytracer_core` contains the camera, scene graph, parser, plugin loader, renderer interfaces and shared render helpers. `raytracer` is a thin `main.cpp`/`App.cpp` executable.
- Plugins are loaded at runtime through `Factory`/`DLManager`. Every plugin target links `src/plugins/PluginABI.cpp` and exports `raytracerPluginABIVersion()`; add new plugin targets to `RAYTRACER_PLUGIN_TARGETS` in `CMakeLists.txt` or the loader rejects them.
- Plugin `create` symbols cross an untyped `dlsym` boundary. Match argument types exactly; for example `PhongMaterial::create` takes `double`, not `float`.
- CPU and CUDA renderers are separate implementations. Shared scene data is flattened through `include/plugins/renderers/SceneSnapshot.hpp`; changing material, primitive or light data usually requires updating the snapshot producer and both renderers.
- The parser is strict: unknown keys are rejected. Update `src/plugins/parser/Parser.cpp`, `docs/scene-format.md` and tests together when adding scene fields.
- Nested scene composition rejects named instances/lights. Prefab-internal lights follow animated instance transforms.
- MTL `d`/`Tr` opacity is unsupported; use dielectric materials for transparency.

## Conventions and gotchas

- C++20, 4-space indentation, Allman braces, include guards, `namespace RayTracer`. The build uses `-Wall -Wextra -Werror`; fix warnings rather than adding suppressions.
- There is no `.clang-format`, `.clang-tidy`, or `.pre-commit-config.yaml`. `./pre-commit.sh` cannot run as documented.
- Do not hand-edit generated `raytracer`, `plugins/`, `build/`, or golden PPMs except when deliberately refreshing goldens.
- Detailed references live in `docs/developer.md`, `docs/scene-format.md`, `docs/cli.md`, `docs/renderers.md`, and `docs/audit.md`. Prefer code/tests when prose disagrees; some older sections predate recent fixes.
