# Changelog

All notable changes to this project are documented here.

## [1.0.0] - 2026-11-17

### Added

- Five-scene MYTHOS pack: The Last Leviathan, Cathedral of the Machine Sun,
  Garden of Impossible Glass, Serpent of Falling Stars and Clockwork Aurora.
- Three 8-second, 24 fps MYTHOS films and a generated contact sheet.
- Procedural shard/petal meshes and eclipse/aurora/forge HDR skies.
- Versioned CMake package (`raytracerConfigVersion.cmake`) and
  `tests/package_consumer.sh` for downstream `find_package` checks.
- `package-consumer` and headless `gui-smoke` CI jobs.
- GUI output parity test: each backend's `--output` image must match the
  deterministic CLI render byte-for-byte.
- Parser fuzz smoke test, PPM reader regressions, dielectric-stack regressions
  and sampling-math regressions.
- `mythos_scenes_smoke` scene coverage.

### Changed

- Split the parser into orchestration, animation, geometry, materials/mesh and
  lights modules.
- Split the CPU renderer into lifecycle/output and sampling/shader modules.
- Split CUDA device code into shared math, intersections and integrator units.
- Shared scalar sampling formulas between CPU and CUDA.
- Prefab internal lights now follow animated instance transforms.
- Dielectric paths now use a per-ray interface stack instead of one boolean.

### Fixed

- Completed the 44-finding audit recorded in `docs/audit.md`, including CPU
  mesh material overrides, CUDA RNG sequencing, height-fog overflow, camera
  auto-focus, HDR sanitisation, parser range checks, output validation,
  temporary-file cleanup, BVH refit reuse, concave OBJ triangulation and
  plugin ABI validation.
- Relaxed the golden-image isolated-pixel guard to tolerate a single HDR
  firefly while keeping mean and p99 structural limits.

### Verification

- 132 tests in the reference build; 131 in one-GUI-backend builds.
- All 47 scenes render on CPU and CUDA in the scene smoke checks.
- Golden images, CPU/CUDA parity, package consumer, Valgrind and
  compute-sanitizer checks pass.
