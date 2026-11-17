# Code audit and remediation tracker

Baseline: `43ffff7 feat: animation completion` — 120 tests, 42 scenes,
15 golden references. This file records every audit finding, its severity,
evidence, remediation status and verification.

Status legend: `OPEN`, `IN PROGRESS`, `DONE`, `DEFERRED`.

Current status: **44 DONE**, **0 DEFERRED**, **132 tests**.

## P0 — correctness

### A01 — CPU mesh material overrides use the wrong BRDF for bounces

- **Status:** DONE
- **Severity:** High
- **Evidence:** `src/plugins/renderers/CPU/Renderer.cpp:629-635` resolves the
  primitive fallback material inside `computeNextRayDirection()` and ignores
  `payload.getMaterialIndex()`.
- **Impact:** A mesh with mixed material groups (for example a metallic group
  on a diffuse fallback) shades with one material but bounces with another.
  CPU/CUDA parity also diverges.
- **Fix:** pass the resolved material type (and roughness) into the bounce
  function.
- **Test:** mixed diffuse/metallic mesh CPU/CUDA parity.

### A02 — CUDA unit-sphere sampling has unsequenced RNG state mutations

- **Status:** DONE
- **Severity:** High
- **Evidence:** `src/plugins/renderers/CUDA/Kernels.cu:171-177` calls
  `randomFloat(state)` three times inside one `makeVec3(...)` expression.
- **Impact:** Unspecified evaluation order plus unsequenced writes is undefined
  behavior; output can change between CUDA compiler versions.
- **Fix:** draw explicit `z`, `y`, `x` locals before constructing the vector.
- **Test:** CUDA determinism and CPU/CUDA parity.

### A03 — Height fog incorrectly becomes transparent on long downward rays

- **Status:** DONE
- **Severity:** High
- **Evidence:** `include/render/AtmosphereSampling.hpp:49-51` returns 0 when
  the analytic depth overflows to infinity; CUDA mirrors it at
  `Kernels.cu:266-268`.
- **Impact:** Dense height fog can disappear instead of becoming opaque.
- **Fix:** propagate positive infinity as the optical depth.
- **Test:** long downward height-fog ray with high falloff.

### A04 — Animated scale can cross zero or become negative

- **Status:** DONE
- **Severity:** High
- **Evidence:** `src/render/SceneAnimation.cpp:87-88` uses Catmull-Rom for
  scale; only keyframe values are validated at `SceneAnimation.cpp:171-178`.
- **Impact:** Overshoot can produce zero/negative scale, a non-invertible
  transform and disappearing or mirrored geometry.
- **Fix:** clamp interpolated scale to a small positive minimum and reject
  non-finite results.
- **Test:** scale track with extreme positive keyframes.

### A05 — Camera auto-focus sentinel is lost

- **Status:** DONE
- **Severity:** Medium/High
- **Evidence:** `src/Camera.cpp:74-78` always stores a positive focus distance;
  `Camera.cpp:278-286` and `CameraController.cpp:108-114` cannot restore
  “auto”.
- **Impact:** Interactive camera movement keeps the old focus distance instead
  of tracking the look-at distance.
- **Fix:** track auto-focus explicitly.
- **Test:** camera-controller focus reset/movement regression.

### A06 — Negative colour components are accepted

- **Status:** DONE
- **Severity:** Medium
- **Evidence:** `src/plugins/parser/Parser.cpp:1442-1452` validates finiteness
  only.
- **Impact:** Negative radiance/throughput can break lighting, bloom and
  tonemapping.
- **Fix:** reject negative material, emission, background and light colours.
- **Test:** negative colour parser negatives.

### A07 — HDR texels are not sanitized

- **Status:** DONE
- **Severity:** Medium
- **Evidence:** `src/render/Texture.cpp:185-208` copies `stbi_loadf` values
  without checking finiteness.
- **Impact:** NaN/Inf texels poison radiance and post-processing.
- **Fix:** replace non-finite HDR components with zero (or clamp).
- **Test:** HDR texture with non-finite values.

### A08 — Plane/cylinder/cone hit epsilon inconsistency

- **Status:** DONE
- **Severity:** Medium
- **Evidence:** `src/plugins/primitives/Plane.cpp:52` accepts `distance >= 0`;
  `Cylinder.cpp:123,153` and `Cone.cpp:87` accept zero-distance hits.
- **Impact:** Self-intersections and shadow acne on surfaces.
- **Fix:** require `distance > Ray::EPSILON` consistently.
- **Test:** surface self-hit regression.

## P1 — robustness and output

### A09 — Unsupported output extensions silently write PPM

- **Status:** DONE
- **Severity:** Medium
- **Evidence:** `src/render/ImageWriter.cpp:51-57`.
- **Fix:** accept only `.ppm` and `.png`; reject everything else.
- **Test:** CLI rejects `.jpg` output.

### A10 — Temporary files leak on image/metadata write failure

- **Status:** DONE
- **Severity:** Medium
- **Evidence:** `src/render/ImageWriter.cpp:72-82`,
  `src/render/SequenceMetadata.cpp:240-260`.
- **Fix:** remove the temporary file when writing throws.
- **Test:** unwritable/failed output cleanup.

### A11 — Temporary names collide across concurrent renders

- **Status:** DONE
- **Severity:** Medium
- **Evidence:** Image, video and metadata writers use a deterministic
  `<stem>.tmp.<ext>` name.
- **Fix:** include PID and a unique counter/random suffix.
- **Test:** two concurrent renders to distinct frames do not interfere.

### A12 — PPM output is per-pixel

- **Status:** DONE
- **Severity:** Medium (performance)
- **Evidence:** `src/render/PPM.cpp:27-36` calls `out.write()` per pixel.
- **Fix:** write row buffers or one contiguous payload.
- **Test:** golden output unchanged.

### A13 — HDR/PPM parsing robustness

- **Status:** DONE
- **Severity:** Medium
- **Evidence:** The P6 loader required exactly one byte after `maxval`, P6
  components were not range-checked, and both GUI backends carried private PPM
  parsers.
- **Fix:** P6 components are validated against the header maximum, HDR values
  are sanitised, the strict binary separator is documented, and both GUI
  backends now use the shared `readPPMFile()` reader. Header comments and P3
  input are covered by the shared texture parser.
- **Test:** `checkPPMReader()` covers commented P6, P3, packing and truncated
  input; the PPM comment/whitespace policy is documented in `Texture.hpp`.

### A14 — Parser integer casts and size limits

- **Status:** DONE
- **Severity:** Medium
- **Evidence:** `src/plugins/parser/Parser.cpp:858-859,1240` use unchecked
  `static_cast<int>`; OBJ/MTL/keyframe/vertex inputs are unbounded.
- **Fix:** validated numeric conversion helpers and sane caps.
- **Test:** malformed/huge input corpus.

### A15 — CLI dimensions and PNG stride lack upper bounds

- **Status:** DONE
- **Severity:** Medium
- **Evidence:** `src/App.cpp:427-433` checks positivity only;
  `ImageWriter.cpp` uses `width * 4` as an int stride.
- **Fix:** cap width/height and pixel counts; use checked arithmetic.
- **Test:** oversized CLI dimensions rejected.

### A16 — Video writer changes global SIGPIPE and leaks CLOEXEC

- **Status:** DONE
- **Severity:** Medium
- **Evidence:** `src/render/VideoWriter.cpp:389-393`.
- **Fix:** scoped signal handling and `O_CLOEXEC`/`FD_CLOEXEC`.
- **Test:** video output smoke test still passes.

### A17 — Parser silently ignores unknown keys

- **Status:** DONE
- **Severity:** Medium (usability)
- **Evidence:** primitive/material loops only look up known names.
- **Fix:** validate unknown keys or emit diagnostics.
- **Test:** typo’d scene key rejected.

## P2 — architecture, physics and performance

### A18 — Prefab internal lights do not follow animated instances

- **Status:** DONE
- **Severity:** Medium
- **Evidence:** `src/scene/SceneBuilder.cpp:151-159` cloned prefab lights once
  with the base pose; `SceneAnimation` only updated instance primitives.
- **Fix:** `Scene` now records prefab lights against their instance group.
  `SceneBuilder` stores an untransformed prototype plus the current light, and
  `SceneAnimation::apply()` rebuilds the current light from that prototype with
  the same transform as the instance primitives. Point/sphere lights translate
  and scale, and directional lights rotate, without accumulating transforms
  across frames. CPU and CUDA renderers already refresh their light data when
  `instancesChanged` is reported.
- **Test:** `checkPrefabLightAnimation()` composes a prefab with an internal
  point light, animates the instance, and verifies the light follows it.

### A19 — Nested scene instance/light IDs are lost during composition

- **Status:** DONE (composition rejects named nested instances/lights)
- **Severity:** Medium
- **Evidence:** `SceneBuilder::addScene()` copies materials/primitives/lights
  but not instance groups or light IDs.
- **Fix:** prefix/copy IDs or reject nested IDs with a clear error.

### A20 — `InstancePrimitive::clone()` silently shares children

- **Status:** DONE
- **Severity:** Medium
- **Evidence:** `src/scene/InstancePrimitive.cpp:89-102`.
- **Fix:** return null or throw when the child cannot clone.

### A21 — Area-light volumetric NEE is biased

- **Status:** DONE (documented approximation)
- **Severity:** Medium (quality)
- **Evidence:** `SphereLight::sample()` returns radiance × solid angle, while
  medium NEE evaluates the phase at one direction.
- **Fix:** the approximation is explicitly documented in
  `docs/renderers.md` under renderer approximations. Medium NEE samples the
  phase at the cone direction rather than integrating it over the full area
  light; full phase-weighted MIS remains future work if volumetric area lights
  become a primary feature.

### A22 — Single `insideDielectric` boolean

- **Status:** DONE
- **Severity:** Medium (quality)
- **Evidence:** CPU `Renderer.cpp` and CUDA `Kernels.cu` used one boolean for the
  whole path, so nested or overlapping dielectrics swapped between air and the
  wrong IOR.
- **Fix:** added `include/render/DielectricStack.hpp`, a fixed-size
  host/device stack keyed by primitive index. Entering an interface pushes its
  IOR; exiting pops it; reflected rays leave the medium unchanged. Both
  renderers use the stack to derive `etaI`/`etaT` from the current surrounding
  medium. The existing single-dielectric behaviour is unchanged.
- **Test:** `checkDielectricStack()` covers entry, nesting, exterior IOR,
  duplicate rejection, capacity and removal. The tidal-bloom animation golden
  was intentionally refreshed because overlapping petals now use the correct
  per-interface media.

### A23 — Blinn-Phong direct light and metallic/emissive overlap

- **Status:** DONE (documented artistic model)
- **Severity:** Medium (quality)
- **Evidence:** direct light loop runs for metallic/emissive materials and uses
  `ambient + diffuse + specular`.
- **Fix:** documented as the deliberate artistic model in `docs/renderers.md`:
  direct lighting is Blinn-Phong, metallic surfaces also receive the direct
  specular term, and emissive surfaces reflect light. Restricting terms per
  material type would change the established look and is left as an optional
  future quality pass rather than an unverified behavioural change.

### A24 — Animated frames rebuild too much per subframe

- **Status:** DONE
- **Severity:** Medium (performance)
- **Evidence:** `CPURenderer.cpp:150-166` constructs a new `Renderer` per
  subframe; `CUDARenderer.cpp:1230` rebuilds the whole snapshot.
- **Fix:** reuse CPU buffers and add a primitives/lights-only snapshot update.

### A25 — PPM/PNG/GUI endianness assumptions

- **Status:** DONE (documented platform assumptions)
- **Severity:** Low/Medium (portability)
- **Evidence:** pixels are stored as `A<<24|B<<16|G<<8|R` and written as bytes.
- **Fix:** document little-endian/desktop-only or convert explicitly.

## P3 — maintainability and test gaps

### A26 — Monolithic parser and renderers

- **Status:** DONE
- **Severity:** Low/Medium
- **Evidence:** `Parser.cpp` was ~1.5k lines, `Renderer.cpp` ~1.1k and
  `Kernels.cu` ~2.3k, mixing unrelated responsibilities in single translation
  units.
- **Fix:** split the parser into orchestration/values (`Parser.cpp`), animation
  (`ParserAnimation.cpp`), geometry (`ParserGeometry.cpp`), materials/mesh
  (`ParserMaterials.cpp`) and lights (`ParserLights.cpp`), with shared helpers
  in `ParserInternal.hpp/.cpp`. Split the CPU renderer into lifecycle/output
  (`Renderer.cpp`) and sampling/shading (`RendererSampling.cpp`). Split CUDA
  device code into `CudaCommon.cuh`, `CudaIntersections.cuh` and the remaining
  integrator/kernel entry points in `Kernels.cu`.
- **Test:** full suite, golden images, CPU/CUDA parity and all 42 scene renders
  pass after the split.

### A27 — CPU/CUDA sampling logic is duplicated

- **Status:** DONE (shared scalar sampling core; vector adapters remain type-specific)
- **Severity:** Medium (maintenance)
- **Evidence:** BSDF, phase, atmosphere and light sampling existed in both
  implementations with independently maintained scalar formulas.
- **Fix:** added `include/render/SamplingMath.hpp` with host/device scalar
  helpers for clamp, Smith GGX, Schlick, Blinn shininess, Henyey-Greenstein
  phase and the power heuristic. CPU `Brdf.hpp` and CUDA
  `CudaCommon.cuh`/`Kernels.cu` now delegate to those helpers. Vector-valued
  sampling remains renderer-specific because CPU uses `Vec3D` and CUDA uses a
  POD `Vec3`; CPU/CUDA parity tests cover the shared formulas end to end.
- **Test:** `checkSamplingMath()` covers the shared formulas; the existing parity
  suite covers renderer integration.

### A28 — Plugin ABI and static DLManager fragility

- **Status:** DONE
- **Severity:** Low/Medium
- **Evidence:** C++ types cross `extern "C"` boundaries; static
  `Factory::_dlManager` was unsynchronized.
- **Fix:** plugin creation is mutex-serialised, and every plugin target now
  links `src/plugins/PluginABI.cpp`, which exports
  `raytracerPluginABIVersion()`. `DLManager::validateABI()` checks the value
  against `RayTracer::PLUGIN_ABI_VERSION` before resolving `create`, so a stale
  plugin fails with a clear diagnostic. The ABI constant is documented in
  `docs/developer.md` and must be bumped when exported signatures or shared
  plugin types change.
- **Test:** `plugin_abi_mismatch_rejected` loads a deliberately versionless
  shared library through `DLManager` and requires rejection for the ABI reason;
  every normal plugin load exercises the matching-version path.

### A29 — Legacy/dead code

- **Status:** DONE (removed randomPixelRayCompute, unused Utils conversions and main.cpp legacy block)
- **Severity:** Low
- **Evidence:** `Camera::randomPixelRayCompute`, `Color4D`,
  `Utils::convertTo255/convertFromRGBA`, `CustomMaterial`, commented
  `main.cpp` block, unused MTL opacity.
- **Fix:** remove or document as legacy.

### A30 — Test coverage gaps

- **Status:** DONE
- **Severity:** Medium
- **Evidence:** The original suite did not cover mixed-material meshes, focus,
  height-fog overflow, extreme scale, self-hits, output failures, unknown keys,
  nested IDs, PPM comments, parser fuzzing or GUI startup.
- **Fix:** Added targeted regressions for each area. Parser fuzzing is covered
  by `tests/parser_fuzz_smoke.sh` (80 deterministic mutations), PPM
  comment/truncation cases by `checkPPMReader()`, and both GUI backends are
  started under `xvfb-run` by `tests/gui_smoke.sh` when a virtual display is
  available. The prefab-light animation limitation is tracked separately under
  A18 rather than asserted as supported behaviour.

## Verification log

- 2026-11-16: audit recorded; baseline 120/120 tests.
- 2026-11-16: P0 items A01-A08 implemented. Added mixed-material mesh parity,
  height-fog overflow and camera-focus regression tests. 124/124 tests pass.
- 2026-11-16: P1 items A09-A12, A15, A16, A20 implemented. Output extensions are
  validated, temporary paths are unique and cleaned on failure, PPM output is
  row-buffered, CLI dimensions are capped, video signal/CLOEXEC handling is
  scoped, and instance clones reject non-cloneable children.
- Intentional golden refresh after the A08 epsilon correction: brass orrery,
  sakura shrine, tidal bloom and misty shrine references.
- 2026-11-16: P1 parser hardening completed. Unknown keys are rejected,
  numeric casts are range-checked, OBJ/MTL/keyframe limits are enforced,
  named nested scenes are rejected, and the cathedral scene's singular
  `sphere` light typo was corrected (intentional golden refresh).
- 2026-11-16: CPU animation reuses one renderer and worker pool per sequence;
  CUDA animation refreshes only camera/primitives/lights and reuses the original
  snapshot's materials, textures, meshes, background and atmosphere.
- 2026-11-16: Full suite: 126/126 tests pass, including golden and CPU/CUDA
  parity.
- 2026-11-16: Plugin factory creation is serialised with a mutex. A13 was
  deferred at that point (later closed), and the remaining architectural items
  were deferred with rationale (all later closed).
- 2026-11-16: Added A31-A41 from the low-priority audit pass, fixed invalid
  AABB bounds, parser cache reset and duplicate MTL names. Scene animation now
  compares transforms before reporting a change, GUI screenshots use atomic
  image writes, `%u` frame paths dispatch correctly, `Vec3D::operator[]` uses
  debug assertions with checked `at()` access, and concave OBJ polygons are
  ear-clipped with a fan fallback. Added a deterministic parser fuzz smoke test
  (80 mutated scenes) and an installable CMake package config. 127/127 tests
  pass on GCC, Clang, ASan/UBSan and isolated CUDA; valgrind and
  compute-sanitizer are clean. The installed package builds and runs a
  downstream `find_package(raytracer CONFIG REQUIRED)` consumer.
- 2026-11-16: Final verification after A31/A36/A38/A40/A44. 127/127 tests pass
  on GCC Release, Clang Release, ASan+UBSan with leak detection, and isolated
  CUDA Release. All 42 scenes render at 32x18 on both CPU and CUDA with no
  failures. Valgrind reports no definite/indirect leaks and compute-sanitizer
  reports 0 errors. The full install exports 118 files; the downstream package
  consumer prints `3.74166 960`.
- 2026-11-16: A13/A41 closed. The two GUI backends now share the core
  `readPPMFile()` reader, which accepts P3/P6 with header comments and rejects
  truncated input. The CLI documentation no longer claims unsupported output
  extensions fall back to PPM. The PPM reader regression covers P6, P3,
  packed-pixel layout and truncation.
- 2026-11-16: A28 closed. Every plugin target exports
  `raytracerPluginABIVersion()`, and `DLManager` rejects missing or mismatched
  versions before resolving `create`. The new `plugin_abi_mismatch_rejected`
  test covers a versionless library.
- 2026-11-16: A35 closed. `App` validation and help now throw
  `RayTracer::AppExit{code}`; `main()` returns that status, so no library or
  application state has to call `std::exit()`. The `help_exits_successfully`
  test covers the success path.
- 2026-11-16: Final verification after A13/A28/A35/A41. 129/129 tests pass on
  GCC Release, Clang Release, ASan+UBSan with leak detection, and isolated CUDA
  Release. All 42 scenes render at 32x18 on both CPU and CUDA with no failures.
  Valgrind reports no definite/indirect leaks and compute-sanitizer reports 0
  errors. The install exports 184 files and the downstream package consumer
  still prints `3.74166 960`.
- 2026-11-16: A30 closed. `tests/gui_smoke.sh` starts both GUI backends under
  `xvfb-run` and requires the first frame to render before the expected
  timeout; the test is added only when `xvfb-run` is available. The reference
  build now runs 132 tests; the Clang, ASan/UBSan and isolated CUDA build
  directories have only one GUI backend enabled, so they run 131.
- 2026-11-16: A26/A27/A43 closed, completing the audit. The parser, CPU
  renderer and CUDA device code are split into focused translation units;
  shared scalar sampling math is used by both renderers; type aliases are
  documented as an accepted design tradeoff. 132/132 tests pass on GCC, with
  131/131 on Clang, ASan/UBSan and isolated CUDA; all 47 scenes render on both
  renderers, valgrind and compute-sanitizer are clean, and the 197-file install
  still satisfies the downstream package consumer.
- 2026-11-17: Added the five-scene MYTHOS pack and three 8-second preview
  films. New shard/petal meshes and eclipse/aurora/forge HDR skies are
  generated by `tests/generate_mythos_assets.py`; the scene configs are
  generated by `tests/generate_mythos_scenes.py`. Added the
  `mythos_scenes_smoke` test, bringing the reference suite to 132 tests.

## Additional low-priority findings

### A31 — `formatFramePath()` passes `int` for `%u`

- **Status:** DONE
- **Severity:** Low
- **Evidence:** `src/render/ImageWriter.cpp` always passes an `int` frame to
  `snprintf`, even when the validated conversion is `%u`.
- **Fix:** dispatch on the conversion character so `%u` receives an
  `unsigned int` and `%d`/`%i` receive an `int`. The frame-path formatting
  regression test covers both forms.

### A32 — `AABB::intersect()` did not reject invalid bounds

- **Status:** DONE
- **Evidence:** `include/scene/AABB.hpp`.
- **Fix:** early `valid()` check.

### A33 — Parser caches were not reset between `init()` calls

- **Status:** DONE
- **Evidence:** `src/plugins/parser/Parser.cpp`.
- **Fix:** clear texture, mesh and MTL caches in `init()`.

### A34 — Duplicate MTL material names were accepted

- **Status:** DONE
- **Evidence:** `src/scene/MtlLoader.cpp`.
- **Fix:** reject duplicate `newmtl` names.

### A35 — `App` validation uses `std::exit`

- **Status:** DONE
- **Severity:** Low
- **Impact:** Harder to unit-test and to reuse the application entry point.
- **Fix:** `App` now throws `RayTracer::AppExit{code}` for help and validation
  failures instead of terminating the process. `main()` maps the structured
  status to the process exit code, and stack unwinding runs destructors for
  partially constructed sessions. `--help` is covered by a success-path test.

### A36 — No installable CMake package config

- **Status:** DONE
- **Severity:** Low
- **Impact:** Downstream projects cannot `find_package(raytracer)`.
- **Fix:** export core targets and headers. `raytracer_core` now installs its
  public include directories, headers land under `include/raytracer`, and the
  exported `RayTracer::raytracer_core` target plus `raytracerConfig.cmake` are
  installed under `share/cmake/raytracer`. A downstream consumer was configured,
  built and run against the installed prefix.

### A37 — Scene animation always reports instance changes

- **Status:** DONE
- **Severity:** Low/Medium (performance)
- **Evidence:** `src/render/SceneAnimation.cpp` sets `instancesChanged = true`
  for every track even when the clamped time/transform is unchanged.
- **Fix:** detect identical transforms/clamped times and skip updates/refits.

### A38 — `Vec3D::operator[]` throws in hot loops

- **Status:** DONE
- **Severity:** Low/Medium (performance)
- **Evidence:** `include/utils/Vec3D.inl`.
- **Fix:** unchecked access in release builds with debug bounds checks.

### A39 — Epsilon thresholds are scene-scale dependent

- **Status:** DONE (documented scene-unit guidance)
- **Severity:** Low/Medium
- **Evidence:** `Ray::EPSILON`, mesh-local determinant epsilon and primitive
  thresholds are absolute values.
- **Fix:** kept the deterministic absolute tolerances and documented the
  supported scene scale in `docs/scene-format.md` ("Scene scale and
  precision"). The guidance covers the `0.01`-`10000` unit range, warns against
  mixing micro and kilometre scales, and explains the self-hit limit. This is
  the documented option from the original finding; relative epsilon scaling is
  intentionally not applied because it would change hit acceptance per ray and
  silently alter existing scenes.

### A40 — OBJ concave polygons use fan triangulation

- **Status:** DONE (ear clipping with fan fallback for non-simple input)
- **Severity:** Low
- **Evidence:** `src/scene/MeshLoader.cpp` triangulates every polygon from its
  first vertex.
- **Impact:** Concave ngons can overlap or render incorrectly.
- **Fix:** ear clipping or documented convex-face requirement.

### A41 — GUI backends duplicate the PPM loader

- **Status:** DONE
- **Severity:** Low
- **Evidence:** `src/plugins/GUI/SFML.cpp` and `src/plugins/GUI/OpenGL.cpp`
  each parsed the P6 header and packed pixels independently.
- **Fix:** both backends now call the shared `readPPMFile()` reader in
  `src/render/PPM.cpp`, which reuses the validated core PPM/texture parser.
  GUI window tests remain manual because the backends need a display.
- **Test:** `checkPPMReader()` covers the shared reader's P6/P3 parsing,
  packed layout and truncated-input rejection.

### A42 — MTL opacity is parsed but not applied

- **Status:** DONE (removed from the public MTL structure)
- **Severity:** Low
- **Evidence:** `MtlMaterial::opacity` was filled by `loadMtl()` but never
  consumed by the importer.
- **Fix:** removed the unused field and the `d`/`Tr` parsing. The MTL loader now
  documents those fields as ignored, and the scene format points users to
  dielectric materials for transparent surfaces instead of silently accepting
  an opacity value that had no effect.

### A43 — Type aliases reduce semantic safety

- **Status:** DONE (accepted design)
- **Severity:** Low (design)
- **Evidence:** `Point3D` and `Color3D` are aliases of `Vec3D`.
- **Impact:** Positions, directions and colours are interchangeable in the
  type system.
- **Fix:** documented in `docs/developer.md` as an intentional tradeoff.
  Points, directions and colours share arithmetic and cross the plugin ABI, so
  distinct wrapper types would add conversion/operator boilerplate without
  preventing likely mistakes. Parameter names and helper signatures carry the
  semantic distinction; revisit if units or colour spaces enter the API.

### A44 — GUI screenshots used non-atomic PPM writes

- **Status:** DONE
- **Evidence:** both renderers' screenshot helpers now use
  `writeImageFileAtomic()`.

## Remaining deferred work

No findings remain deferred. The audit is closed.
