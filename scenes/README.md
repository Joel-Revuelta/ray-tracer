# Raytracer test scenes

Each file is a complete scene for the parser. See
[../README.md](../README.md) for building and running the project,
[../docs/cli.md](../docs/cli.md) for the complete command-line reference, and
[../docs/scene-format.md](../docs/scene-format.md) for the full scene
configuration reference.

Run one interactively with:

```bash
./raytracer --path scenes/<name>.cfg --gui sfml
./raytracer --path scenes/<name>.cfg --gui opengl
```

Or render it headlessly without a display server:

```bash
./raytracer --path scenes/<name>.cfg --cli --width 960 --height 540 --frames 30 --output render.ppm
```

Useful options:

- `--cli` — render without opening a window. The image is written to
  `--output`, or to standard output when `--output` is omitted.
- `--gui sfml|opengl` — open an interactive window. `--sfml` is an alias for
  `--gui sfml`. A file is only written when `--output` is given.
- `--samples N` — samples per pixel (higher = cleaner, slower; default 10).
  Use 30+ for quick previews and 100+ for final stills. `--frames` is a
  backward-compatible alias.
- `--output out.ppm|out.png` — write a binary P6 PPM or 8-bit RGBA PNG.
- `--width/--height` — render resolution, GUI window size and output size
  (default 960x540).
- `--renderer cpu|cuda` — choose the renderer plugin (default `cpu`).
- `--list-renderers` — list the available renderer plugins and their status.
- `--help` — print usage and exit.

While the window is open: `WASD` moves relative to the current view, `Q/E` moves
up/down, arrow keys or `IJKL` look around, `+/-` zooms, `R` restores the starting
pose, `X` locks the look-at point, `C` locks the camera position, and `Esc` quits.
`Space` pauses/resumes, `P` saves a screenshot, `[`/`]` changes exposure,
`,`/`.` changes the lens radius, and `;`/`'` pulls/pushes focus.
Resizing the SFML window stretches the rendered image; the OpenGL backend
letterboxes it to preserve aspect ratio.

| Scene | What it shows |
| --- | --- |
| `showcase.cfg` | Diffuse, metallic, emissive and flat materials together, three light types |
| `primitives.cfg` | Spheres, a cylinder, a cone and an infinite plane |
| `materials.cfg` | The four material models side by side on identical spheres |
| `reflections.cfg` | A metallic mirror floor with coloured metallic spheres |
| `lighting.cfg` | Red, green and blue point lights plus visible emissive bulbs |
| `orbits.cfg` | A small planetary scene with an emissive sun, planets and a starfield |
| `neon_district.cfg` | A stylized neon boulevard with light-banded towers and a glowing amber orb |
| `crystal_garden.cfg` | Glowing crystal spires, metallic shards and floating orbs on a mirrored floor |
| `chrome_singularity.cfg` | A chrome world wrapped in a tilted accretion ring and a starfield |
| `solar_bloom.cfg` | A huge amber sun rising over a mirror sea with silhouetted spheres |
| `hypernova_cathedral.cfg` | A chrome singularity ringed by neon particles inside a cosmic colonnade |
| `infinity_mirror.cfg` | A mirrored corridor filled with repeating neon gates and endless reflections |
| `crystal_superbloom.cfg` | A vast field of giant crystal spires, floating orbs and a distant moon |
| `solar_eclipse.cfg` | A giant amber sun eclipsed by a dark sphere above a mirror sea |
| `glass_menagerie.cfg` | Dielectric spheres and a glass column with chrome, gold and emissive orbs |
| `prism_pavilion.cfg` | A glass colonnade with glowing cores, a chrome orb and a neon ring |
| `aurora_lens.cfg` | A giant glass lens, rose glass and a floating aurora arc |
| `roughness_study.cfg` | Metal spheres from perfect mirror to satin roughness |
| `transforms.cfg` | Translate/rotate/scale transforms on spheres, cylinders, cones and planes |
| `meshes.cfg` | Instanced, smooth-shaded OBJ icospheres and a torus with shared geometry |
| `depth_of_field.cfg` | Five spheres at increasing depths with a focused middle plane |
| `soft_shadows.cfg` | Two sphere area lights over a floor and three spheres |
| `textures.cfg` | Checker PPM textures on a plane, primitives and a mesh |
| `material_maps.cfg` | Roughness, normal and emission maps on metal, stone and an emissive sign |
| `primitives_extra.cfg` | Axis-aligned boxes and metal/glass disks/rings |
| `scatter_field.cfg` | 180 deterministically scattered emissive spores |
| `misty_shrine.cfg` | Torii gate and lanterns in low exponential height fog |
| `volumetric_cathedral.cfg` | Strong god rays through a foggy cathedral nave |
| `lagoon_mist.cfg` | Glowing blooms and reflections under thin water haze |
| `motion_blur.cfg` | Fast emissive orb and area light with object/light motion blur |
| `obj_materials.cfg` | A multi-material OBJ cube with `usemtl` groups and a fallback |
| `environment.cfg` | An equirectangular sky used for reflections and IBL |
| `mtl_materials.cfg` | Automatic OBJ/MTL material import, including a textured face |
| `mtl_maps.cfg` | MTL `map_Ks` roughness and `map_Ke` emission import |
| `group_transform.cfg` | A nested scene of spheres and a point light transformed as a group |
| `prefabs.cfg` | One lamp prefab instantiated three times with different transforms |
| `sakura_shrine.cfg` | Blue-hour shrine with torii, cherry trees and warm lanterns; camera dolly and pulsing shrine light |
| `brass_orrery.cfg` | Brass orbit rings, glass planets and a glowing sun; camera orbit, rotating rings and sun pulse |
| `tidal_bloom.cfg` | Bioluminescent glass flowers over a mirror lagoon; camera glide, bobbing blooms and pulsing lights |
| `obsidian_aurora.cfg` | Monolith silhouettes under luminous aurora ribbons |
| `frozen_cathedral.cfg` | Ice pillars, a glowing altar and a reflective frozen nave |
| `last_leviathan.cfg` | Chrome-and-glass sea serpent coiled beneath an eclipse halo |
| `cathedral_machine_sun.cfg` | Golden machine sun suspended in a nave of dark marble pillars |
| `impossible_glass_garden.cfg` | Crystal blooms, glowing rings and bokeh spores over a mirror pond; animated bloom sequence |
| `serpent_of_stars.cfg` | Crystal serpent spiraling through aurora shards; animated camera and serpent |
| `clockwork_aurora.cfg` | Brass-and-glass orbital engine rotating in an aurora sky; animated rings and camera |

## MYTHOS scene pack

The MYTHOS pack adds five heavily layered showcase scenes. The stills are tuned
for ACES tone mapping and bloom:

```bash
# The Last Leviathan
./raytracer --path scenes/last_leviathan.cfg --cli --renderer cuda \
  --width 1920 --height 1080 --samples 256 --tonemap aces --exposure 0.95 \
  --bloom --bloom-threshold 2.5 --bloom-intensity 0.30 --output leviathan.png

# Cathedral of the Machine Sun
./raytracer --path scenes/cathedral_machine_sun.cfg --cli --renderer cuda \
  --width 1920 --height 1080 --samples 256 --tonemap aces --exposure 1.0 \
  --bloom --bloom-threshold 2.5 --bloom-intensity 0.22 --output cathedral.png

# Garden of Impossible Glass
./raytracer --path scenes/impossible_glass_garden.cfg --cli --renderer cuda \
  --width 1920 --height 1080 --samples 256 --tonemap aces --exposure 0.95 \
  --bloom --bloom-threshold 2.8 --bloom-intensity 0.25 --output garden.png
```

The three films use the same look with 24 fps, 8 seconds, 960x540, motion blur
and H.264:

```bash
./raytracer --path scenes/impossible_glass_garden.cfg --cli --renderer cuda \
  --width 960 --height 540 --samples 12 --tonemap aces --exposure 0.95 \
  --bloom --bloom-threshold 2.8 --bloom-intensity 0.25 --animation \
  --start-frame 0 --end-frame 191 --shutter-angle 180 --shutter-samples 4 \
  --video garden.mp4
```

Rendered previews and films live in `screenshots/mythos/`.

## Hero scene pack

The five newest scenes use HDR IBL and are tuned for ACES tone mapping plus
bloom: `sakura_shrine` at exposure `1.25`, `brass_orrery` at `0.85`,
`tidal_bloom` at `1.15`, `obsidian_aurora` at `1.15`, and
`frozen_cathedral` at `0.90`. For example:

```bash
./raytracer --path scenes/tidal_bloom.cfg --cli --renderer cuda \
  --width 1920 --height 1080 --frames 320 --tonemap aces --exposure 1.15 \
  --bloom --bloom-threshold 2.5 --bloom-intensity 0.20 --bloom-radius 6 \
  --output tidal_bloom.ppm
```

`obsidian_aurora` is composed for 8K; the other four are composed for 16:9 HD.

## Animations

`sakura_shrine`, `brass_orrery` and `tidal_bloom` include `animation` blocks.
They animate the camera, named prefab instances and light intensities. Still
renders are unchanged; pass `--animation` to play the timeline.
`--start-frame`, `--end-frame` and `--fps` select a sub-range and can override
the scene frame rate. `--shutter-angle` and `--shutter-samples` add camera
motion blur, `--resume` skips frames already on disk, and `--validate` prints
the resolved settings without rendering.

```bash
# Brass Orrery: 8-second arc at 24 fps, full sequence
mkdir -p frames
./raytracer --path scenes/brass_orrery.cfg --cli --renderer cuda \
  --width 1920 --height 1080 --samples 128 --tonemap aces --exposure 0.85 \
  --bloom --bloom-threshold 2.5 --bloom-intensity 0.20 \
  --animation --output 'frames/orrery-%04d.png'

# Sakura Shrine: first 2 seconds only, 12 fps preview
./raytracer --path scenes/sakura_shrine.cfg --cli --renderer cpu \
  --width 640 --height 360 --samples 8 \
  --start-frame 0 --end-frame 23 --fps 12 \
  --output 'frames/sakura-%04d.ppm'

# Half-frame motion blur, resumable after an interruption
./raytracer --path scenes/sakura_shrine.cfg --cli --renderer cuda \
  --width 1280 --height 720 --samples 64 --animation \
  --shutter-angle 180 --shutter-samples 8 --resume \
  --output 'frames/sakura-%04d.png'

# Encode directly to MP4; add --keep-frames to retain the source frames
./raytracer --path scenes/sakura_shrine.cfg --cli --renderer cuda \
  --width 1280 --height 720 --samples 64 --animation \
  --shutter-angle 180 --shutter-samples 8 \
  --video sakura.mp4

# WebM/VP9 is selected by the extension
./raytracer --path scenes/tidal_bloom.cfg --cli --renderer cuda \
  --width 1280 --height 720 --samples 64 --animation \
  --video tidal.webm
```

Video output requires the `ffmpeg` executable (or `RAYTRACER_FFMPEG`).
`--resume` is not supported with video; use the frame-sequence commands above
for resumable renders.

Assemble PNG frames with `ffmpeg` after rendering, or use
`scenes/render_animation.sh`:

```bash
SCENE=sakura_shrine SAMPLES=64 WIDTH=1280 HEIGHT=720 ./scenes/render_animation.sh
FFMPEG=1 ./scenes/render_animation.sh brass_orrery

# Direct video encoding; KEEP_FRAMES=1 also writes the frame sequence
VIDEO=sakura.mp4 KEEP_FRAMES=0 ./scenes/render_animation.sh sakura_shrine
```

All render scripts accept `RAYTRACER_BIN` when the executable lives outside the
default `./raytracer` location:

```bash
RAYTRACER_BIN=./build/raytracer ./scenes/render_hd.sh
```

## Material showcase renders

The glass scenes and roughness study were rendered at `1280×720` with 256 CUDA
samples:

[![Material showcase contact sheet](../screenshots/new-scenes/contact-640.png)](../screenshots/new-scenes/contact.png)

Full-size previews and a contact sheet are in
[`screenshots/new-scenes/`](../screenshots/new-scenes/).

## Full HD renders

`neon_district`, `crystal_garden`, `chrome_singularity` and `solar_bloom` are
composed for 16:9. `scenes/render_hd.sh` renders them at `1920x1080` with 140
frames by default. CPU images go to `screenshots/hd/`; non-CPU renderers go to
`screenshots/hd-<renderer>/` unless `OUT_DIR` is set explicitly.

```bash
./scenes/render_hd.sh
FRAMES=240 ./scenes/render_hd.sh solar_bloom
RENDERER=cuda FRAMES=240 ./scenes/render_hd.sh solar_bloom
```

Set `WIDTH`, `HEIGHT`, `FRAMES`, `RENDERER` or `OUT_DIR` in the environment to
override the defaults.

## 8K renders

The four ultra scenes are composed for 8K:

```bash
./scenes/render_8k.sh
FRAMES=4096 ./scenes/render_8k.sh solar_eclipse
```

The script defaults to `7680x4320`, `2048` frames and the `cuda` renderer. It
writes binary PPM images and converts them to full-size PNGs plus 4K and 1080p
previews in `screenshots/8k/`. A `contact.png` sheet is also created.

Use `FRAMES=4096` or `FRAMES=8192` for even longer passes. The mirrored
Infinity Mirror scene is the most expensive because of its many reflections.

8K rendering is intended for `--cli`; use the 4K or 1080p previews to inspect
the images without loading the full-resolution files.

## Renderers

Rendering is provided by plugins. The default `cpu` renderer is the
multithreaded CPU path tracer. `cuda` is the GPU renderer; it is available when
the project is built with CUDA support. CUDA renders the same scene, lighting
and camera controls as the CPU renderer and is much faster.

```bash
./raytracer --path scenes/showcase.cfg --cli --renderer cpu
./raytracer --path scenes/showcase.cfg --cli --renderer cuda
./raytracer --list-renderers
```

The CUDA versions of the four hero scenes are in `screenshots/hd-cuda/`
(rendered with 1000 frames); the CPU versions remain in `screenshots/hd/`.
CUDA is fast enough that `FRAMES=1000` is practical for very clean 1080p stills.

## Scene format

The full reference is in [../docs/scene-format.md](../docs/scene-format.md).
The short version:

- **Materials** (`materials`): `diffuse` (`ambientReflection`, `diffuseReflection`,
  `color`), `metallic` (`color`), `emissive` (`intensity`, `color`,
  `emissionColor`) and `flatColor` (`color`).
- **Primitives** (`primitives`): `spheres` (`r`, `position`, `material`),
  `planes` (`normal`, `position`, `material`), `cylinders` (`r`, `center` =
  bottom cap, `height`, `material`) and `cones` (`r`, `center` = apex, `h`,
  `material`).
- **Lights** (`lights`): `ambient` (`intensity`, `color`), `directional`
  (`position` = direction towards the light, `intensity`, `color`) and `point`
  (a list of `position`, `intensity`, `color`, `attenuation`).
- **Nested scenes** (`scene`): a `scene` block inside `primitives` is merged
  into the parent scene. Its materials are appended to the parent's material
  list, its primitive material indices are shifted to match, and its lights
  are added to the parent's lights.

Materials are referenced by their index in the `materials` group, starting at 0.
The camera's `rotation` field is the point the camera looks at.

Point lights use inverse attenuation:

```text
intensity / (constant + linear * distance + quadratic * distance^2)
```

The `attenuation` block is required and must contain `constant`, `linear` and
`quadratic` values.

The parser is strict: a missing or mistyped setting, an unknown material type,
a non-positive sphere/cylinder/cone radius or height, a zero plane normal or
light direction, a field of view outside `(0, 180)`, a camera position equal to
its look-at point, or an out-of-range material index prints an error and the
program exits with a non-zero status without writing an image. `metallic` also
accepts the legacy spelling `metalic`. Dielectrics support a positive `ior`
value (default `1.5`).
