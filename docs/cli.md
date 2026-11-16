# CLI reference

The `raytracer` executable parses a libconfig scene, selects a renderer plugin
and either renders headlessly or opens a GUI window.

## Synopsis

```text
./raytracer --path <scene.cfg> --cli|--gui sfml|opengl [options]
```

At least one of `--cli`, `--gui` or `--sfml` is required. `--help` and
`--list-renderers` work without a scene. `--sfml` is an alias for
`--gui sfml`.

## Options

| Option | Argument | Description | Default |
| --- | --- | --- | --- |
| `--path` | scene file | Scene configuration to load. | required |
| `--cli` | — | Headless rendering. | |
| `--gui` | `sfml` or `opengl` | Select the interactive GUI backend. | `sfml` |
| `--sfml` | — | Alias for `--gui sfml`. | |
| `--renderer` | `cpu` or `cuda` | Renderer plugin name. Case-insensitive. | `cpu` |
| `--list-renderers` | — | Print renderer availability and exit `0`. | |
| `--width` | positive integer | Render width, window width and output width. | `960` |
| `--height` | positive integer | Render height, window height and output height. | `540` |
| `--samples` | positive integer | Samples per pixel. | `10` |
| `--frames` | positive integer | Backward-compatible alias for `--samples`. | `10` |
| `--threads` | non-negative integer | CPU worker threads; `0` means automatic. | `0` |
| `--seed` | non-negative integer | Sampling seed offset. | `0` |
| `--device` | non-negative integer | CUDA device index. | `0` |
| `--lens-radius` | non-negative float | Camera lens disk radius; `0` is a pinhole. | scene value |
| `--focus-distance` | positive float | Distance from the eye to the focus plane. | scene value |
| `--exposure` | non-negative float | Linear exposure multiplier before tonemapping. | `1.0` |
| `--tonemap` | `none`, `reinhard`, or `aces` | Output tonemapping curve. | `none` |
| `--bloom` | — | Enable the optional HDR bloom/glow pass. | off |
| `--bloom-threshold` | non-negative float | Linear luminance above which pixels glow. | `1.0` |
| `--bloom-intensity` | non-negative float | Additive bloom strength. | `0.15` |
| `--bloom-radius` | positive integer, max `16` | Gaussian blur radius in pixels. | `8` |
| `--animation` | — | Render the scene's camera animation as a frame sequence. | off |
| `--start-frame` | non-negative integer | First frame of an animation range, inclusive. | `0` |
| `--end-frame` | non-negative integer | Last frame of an animation range, inclusive. | last frame |
| `--fps` | positive float | Override the scene animation frame rate. | scene value |
| `--shutter-angle` | float in `[0, 360)` | Camera shutter opening in degrees; `0` disables motion blur. | scene value |
| `--shutter-samples` | positive integer | Deterministic subframe count for motion blur. | scene value |
| `--resume` | — | Skip animation frames whose output files already exist. | off |
| `--validate` | — | Parse the scene/animation, print a summary and exit without rendering. | off |
| `--video` | video path | Encode the animation to `.mp4`, `.m4v`, `.mov`, `.mkv` or `.webm`. | off |
| `--video-codec` | `h264` or `vp9` | Encoder family; defaults by container. | `h264`, or `vp9` for WebM |
| `--video-crf` | integer | Quality target; H.264 `0–51`, VP9 `0–63`. | `18` / `32` |
| `--video-preset` | preset name | H.264 speed/quality preset. | `medium` |
| `--video-bitrate` | rate | Target bitrate such as `800k` or `8M`; overrides CRF. | unset |
| `--keep-frames` | — | Also write numbered frames when using `--video`. | off |
| `--output` | path or pattern | PPM/PNG output file, or a frame pattern for animation. | stdout in CLI mode; `frame-%04d.ppm` for animation |
| `--list-guis` | — | Print GUI backends and exit `0`. | |
| `--help`, `-h` | — | Print usage and exit `0`. | |

### Samples

`--samples N` (or the legacy `--frames N`) means **N samples per pixel**. Each
sample traces a camera ray and up to 15 bounces. More samples reduce Monte
Carlo noise but scale render time roughly linearly. Frames in an animation are
not controlled by this flag; use `--start-frame` and `--end-frame`.

### Animation

Camera animations are declared in the scene file; see
[scene-format.md](scene-format.md#animation). Animation is CLI-only in
this version.

- `--animation` renders the scene animation.
- `--start-frame`, `--end-frame`, `--fps`, `--shutter-angle` and
  `--shutter-samples` also imply `--animation`.
- `--start-frame` and `--end-frame` are inclusive and zero-based. The default
  range is every frame.
- Frame count is `ceil(duration * fps)`. The per-frame time is `frame / fps`.
- Frames outside the valid range are rejected.
- `--animation` on a scene without an `animation` block is an error.
- A GUI render with animation flags is rejected; use `--cli`.
- Each frame uses a deterministic seed derived from `--seed` and the frame
  index, so CPU and CUDA sequences are reproducible and independent of thread
  count.
- `--lens-radius` and `--focus-distance` override the keyframed values for every
  frame. Leave them unset to use the animation timeline.
- Scenes can also declare animated prefab instances and lights. They play
  alongside the camera; still renders ignore them and use the base scene.
- Motion blur samples the camera, animated instance transforms and animated
  light values at every shutter subframe.
- `--shutter-angle 0` disables motion blur, even when the scene enables it.
  Otherwise, motion blur renders `--shutter-samples` deterministic subframes per
  frame and averages them in linear HDR before post-processing. Render cost
  scales with the sample count.
- `--resume` skips frames whose final output file already exists and is
  non-empty. Frames are written through a temporary file and renamed into place,
  so interrupted renders never leave a partial final image. If every frame
  already exists, the existing metadata sidecar is left untouched.
- `--validate` prints the resolved resolution, samples, duration, frame count,
  output pattern, shutter settings and scene-animation targets, then exits
  without rendering. It also reports an available animation when no animation
  flags were passed.

For an animation output pattern such as `frames/orrery-%04d.png`, the renderer
also writes `frames/orrery.json` after the run. Keep the frame conversion in
the filename so the sidecar name is predictable. The sidecar records a schema
version, the scene, renderer, resolution, samples, seed, FPS, frame range,
shutter and post-process settings, plus the interpolated camera pose, path and
seed for every frame. When scene animation is active, `sceneAnimation`
lists the animated instance and light targets. It is updated when `--resume`
completes a partially rendered sequence.

The renderer prints per-frame progress with elapsed time and an ETA.

### Video output

`--video <path>` encodes the rendered animation directly to a video file. It
implies `--animation` and streams RGBA frames to the `ffmpeg` executable.

```bash
./raytracer --path scenes/brass_orrery.cfg --cli --renderer cuda \
  --width 1920 --height 1080 --samples 128 --animation \
  --shutter-angle 180 --shutter-samples 8 \
  --video orrery.mp4
```

- Supported containers: `.mp4`, `.m4v`, `.mov`, `.mkv` and `.webm`.
- MP4/MOV/MKV default to H.264; WebM defaults to VP9.
- Video-only mode writes no frame files. Add `--output 'frames/f-%04d.png'` or
  `--keep-frames` to write them as well.
- `--keep-frames` without `--output` uses `frame-%04d.ppm`.
- `--resume` is rejected with `--video`; resuming a partially encoded stream is
  not reliable. Render frames with `--output` and encode afterwards when
  resumability matters.
- `--output` is for frame sequences. A video extension in `--output` is rejected
  with a hint to use `--video`.
- Odd widths and heights are padded to even dimensions for `yuv420p`
  compatibility.
- Video is intended for HD and 4K output. 8K video is limited by the encoder
  and pipe bandwidth; use a frame sequence for the master render.
- A JSON sidecar is written next to the video or frame pattern, recording the
  video settings and per-frame camera metadata.

FFmpeg must be installed at render time. The renderer searches `PATH` and
honours `RAYTRACER_FFMPEG` for a non-standard binary. `--validate` reports the
resolved FFmpeg path and video settings without encoding.

```bash
# MP4 with explicit quality settings
./raytracer --path scenes/sakura_shrine.cfg --cli --renderer cpu \
  --width 1280 --height 720 --samples 32 --animation \
  --video sakura.mp4 --video-crf 20 --video-preset slow

# WebM/VP9 plus the numbered PNG frames
./raytracer --path scenes/tidal_bloom.cfg --cli --renderer cuda \
  --width 1280 --height 720 --samples 32 --animation \
  --video tidal.webm --keep-frames
```

Output paths support one printf-style integer conversion, such as `%d` or
`%04d`. A path without a conversion gets a zero-padded `-NNNN` suffix before
the extension. Escape a literal percent sign as `%%`; paths with more than one
conversion are rejected.

```bash
# 1920x1080 PNG sequence into an existing frames/ directory
mkdir -p frames
./raytracer --path scenes/brass_orrery.cfg --cli --renderer cuda \
  --width 1920 --height 1080 --samples 128 --animation \
  --output 'frames/orrery-%04d.png'

# Preview only frames 0-47 at 12 fps
./raytracer --path scenes/sakura_shrine.cfg --cli --renderer cpu \
  --width 640 --height 360 --samples 8 \
  --start-frame 0 --end-frame 47 --fps 12 \
  --output 'frames/sakura-%04d.ppm'

# Half-frame motion blur with 8 subframes
./raytracer --path scenes/sakura_shrine.cfg --cli --renderer cuda \
  --width 1280 --height 720 --samples 64 --animation \
  --shutter-angle 180 --shutter-samples 8 \
  --output 'frames/sakura-%04d.png'

# Validate first, then resume an interrupted sequence
./raytracer --path scenes/brass_orrery.cfg --cli --animation \
  --shutter-angle 180 --shutter-samples 8 \
  --output 'frames/orrery-%04d.png' --validate
./raytracer --path scenes/brass_orrery.cfg --cli --renderer cuda \
  --width 1920 --height 1080 --samples 128 --animation --resume \
  --shutter-angle 180 --shutter-samples 8 \
  --output 'frames/orrery-%04d.png'
```

### Output

- PPM and PNG values are converted from linear radiance through optional bloom,
  exposure, the selected tonemapping curve, and the sRGB transfer function.
- Bloom is applied to the HDR image before exposure and tonemapping, with a
  shared Gaussian kernel on CPU and CUDA.
- The output format follows the extension: `.ppm` writes binary `P6`, `.png`
  writes 8-bit RGBA. Other extensions are rejected before rendering.
- CLI mode with `--output` writes one file per render, or one numbered file per
  animation frame. Final files are written atomically via a temporary file.
- CLI mode without `--output` writes a still PPM to standard output. Animation
  without `--video` defaults to `frame-%04d.ppm` in the current directory and
  writes a `frame.json` metadata sidecar. Video-only renders write no frames.
- GUI mode writes an image only when `--output` is provided; interactive
  `P` screenshots remain timestamped PPM files.
- Parent directories are not created automatically.
- On errors, the program exits with status `1` and does not write an image.

### Renderer selection

```bash
./raytracer --list-renderers
./raytracer --path scenes/showcase.cfg --cli --renderer cpu
./raytracer --path scenes/showcase.cfg --cli --renderer cuda
```

`--list-renderers` reports missing plugins and unavailable devices. If CUDA is
not built or no CUDA device is available, use `cpu` or rebuild with CUDA enabled.

## GUI selection

```bash
./raytracer --path scenes/showcase.cfg --gui sfml --renderer cuda
./raytracer --path scenes/showcase.cfg --gui opengl --renderer cuda
./raytracer --path scenes/showcase.cfg --sfml --renderer cuda
```

The `opengl` backend uses GLFW with an OpenGL 3.3 core context and preserves
the image aspect ratio. The `sfml` backend remains the default. If a selected
GUI plugin was not built, the program reports the missing plugin and exits.

## Exit codes

| Code | Meaning |
| --- | --- |
| `0` | Success, help, or renderer listing. |
| `1` | Invalid arguments, scene load failure, renderer unavailable, output failure, or render failure. |

## GUI controls

| Key | Action |
| --- | --- |
| `W` / `S` | Move forward/backward relative to the current view. |
| `A` / `D` | Strafe left/right. |
| `Q` / `E` | Move up/down in world space. |
| `I` / Up | Look up. |
| `K` / Down | Look down. |
| `J` / Left | Look left. |
| `L` / Right | Look right. |
| `+` / `=` | Zoom in (decrease field of view). |
| `-` | Zoom out (increase field of view). |
| `R` | Restore the initial camera position, look target and field of view. |
| `X` | Lock/unlock the look-at point. |
| `C` | Lock/unlock the camera position. |
| `Space` | Pause/resume sample rendering. |
| `P` | Save the current frame as `raytracer-<timestamp>.ppm`. |
| `[` / `]` | Decrease/increase exposure live. |
| `,` / `.` | Decrease/increase the lens radius. |
| `;` / `'` | Pull/push the focus distance. |
| `Esc` | Close the window. |

Movement is camera-relative: `W`/`S` follow the horizontal view direction and
`A`/`D` follow the camera's right vector. `Q`/`E` remain world-space vertical
movement. `X` keeps the look-at point fixed while the camera moves; `C` keeps the
camera position fixed while the look target moves.

The window title shows resolution, completed/total samples, elapsed time,
exposure, pause state, and the lens/focus values when the lens is non-zero.
Interactive screenshots are written to the current working directory.
`R` restores the initial pose, field of view, lens radius and focus distance.

The SFML backend stretches the rendered image to the new window size. The
OpenGL backend preserves the image aspect ratio with black letterbox bars.
The CUDA renderer updates progressively and processes input continuously while
a sample is rendering.

For 8K or very large renders, use `--cli`; opening a 7680×4320 GUI window is not
practical on most displays.

## Examples

### CPU preview

```bash
./raytracer --path scenes/showcase.cfg --cli \
  --renderer cpu --width 960 --height 540 --samples 30 \
  --output preview.png
```

### Camera animation

```bash
./raytracer --path scenes/brass_orrery.cfg --cli \
  --renderer cuda --width 1280 --height 720 --samples 64 \
  --animation --output 'frames/orrery-%04d.png'

ffmpeg -framerate 24 -i 'frames/orrery-%04d.png' \
  -c:v libx264 -pix_fmt yuv420p orrery.mp4
```

### CUDA high-quality still

```bash
./raytracer --path scenes/solar_eclipse.cfg --cli \
  --renderer cuda --width 1920 --height 1080 --frames 1000 \
  --output eclipse.ppm
```

### Interactive CUDA window

```bash
./raytracer --path scenes/chrome_singularity.cfg --sfml --renderer cuda
```

### Write PPM to stdout

```bash
./raytracer --path scenes/config.cfg --cli --renderer cpu \
  --width 640 --height 360 --frames 4 > render.ppm
```

### 8K script

```bash
./scenes/render_8k.sh
FRAMES=4096 ./scenes/render_8k.sh solar_eclipse
RENDERER=cuda FRAMES=4096 OUT_DIR=/tmp/cuda-8k ./scenes/render_8k.sh solar_eclipse
```

## Render script environment variables

All render scripts accept `RAYTRACER_BIN` to point at a built executable. It
defaults to `./raytracer`, then falls back to `./build/raytracer` when the
in-source layout is not present.

### `scenes/render_hd.sh`

| Variable | Default |
| --- | --- |
| `WIDTH` | `1920` |
| `HEIGHT` | `1080` |
| `FRAMES` | `140` |
| `RENDERER` | `cpu` |
| `OUT_DIR` | `screenshots/hd` for CPU, `screenshots/hd-<renderer>` otherwise |

### `scenes/render_8k.sh`

| Variable | Default |
| --- | --- |
| `WIDTH` | `7680` |
| `HEIGHT` | `4320` |
| `FRAMES` | `2048` |
| `RENDERER` | `cuda` |
| `OUT_DIR` | `screenshots/8k` |
| `CONVERT` | `1`; set to `0` to skip PNG/preview conversion |

### `scenes/render_animation.sh`

| Variable | Default |
| --- | --- |
| `SCENE` or first argument | `brass_orrery` |
| `SAMPLES` | `128` |
| `WIDTH` | `1280` |
| `HEIGHT` | `720` |
| `RENDERER` | `cpu` |
| `FPS` | scene value |
| `START_FRAME` | `0` |
| `END_FRAME` | last frame |
| `SHUTTER_ANGLE` | scene value |
| `SHUTTER_SAMPLES` | scene value |
| `RESUME` | `0`; set to `1` to skip existing frames |
| `VALIDATE` | `1`; set to `0` to skip the pre-render validation pass |
| `VIDEO` | unset; set to a path to encode video directly |
| `KEEP_FRAMES` | `1`; set to `0` for video-only output |
| `OUT_DIR` | `animations/<scene>` |
| `PATTERN` | `<OUT_DIR>/frame-%04d.png` |
| `FFMPEG` | `0`; set to `1` to assemble an MP4 from frames when `VIDEO` is unset |
| `TONEMAP`, `EXPOSURE`, `BLOOM`, `BLOOM_THRESHOLD`, `BLOOM_INTENSITY` | unset; passed through when set |

## Renderer environment variables

| Variable | Effect |
| --- | --- |
| `RAYTRACER_DISABLE_BVH` | Set to a non-zero value to force the linear primitive scan. Useful for benchmarks and parity tests. |
| `RAYTRACER_BVH_STATS` | Set to a non-zero value to print bounded/unbounded counts, node count and build time to stderr. |
| `RAYTRACER_PLUGIN_DIR` | Override the plugin directory (also used by the install bundle). |
| `RAYTRACER_FFMPEG` | Override the `ffmpeg` executable used for video output. |

See [performance.md](performance.md) for benchmark methodology and numbers.

## Common errors

| Message | Resolution |
| --- | --- |
| `Unknown or incomplete option.` | Check the option spelling and required argument. |
| `Invalid width/height/samples value` | Values must be positive integers. |
| `Scene has no animation block: ...` | Add an `animation` block or drop the animation flags. |
| `Animation frame range ... is outside ...` | Check `--start-frame`/`--end-frame` against the scene duration and `--fps`. |
| `Animation rendering is only available with --cli.` | Sequence rendering is headless-only. |
| `Shutter angle must be less than 360 degrees.` | Use an angle in `[0, 360)`; `0` disables motion blur. |
| `Shutter samples must be positive.` | Use `--shutter-samples 1` or more. |
| `Failed to finalize metadata file: ...` | Check that the output directory exists and is writable. |
| `ffmpeg was not found; install it or set RAYTRACER_FFMPEG.` | Install FFmpeg or point `RAYTRACER_FFMPEG` at the binary. |
| `--resume cannot be used with --video` | Render frames with `--output` for resumable workflows. |
| `--output is for frame sequences; use --video` | Pass video paths through `--video`, not `--output`. |
| `WebM output requires the vp9 codec` | Use `--video-codec vp9` or an MP4/MKV container. |
| `ffmpeg stopped while receiving frames` | Check the FFmpeg encoder install and the error output. |
| `Unknown renderer: <name>` | Use `--list-renderers` to see valid names. |
| `Renderer '<name>' is unavailable: ...` | The plugin is missing or the runtime/device is unavailable. |
| `Cannot open output file: ...` | Ensure the parent directory exists and is writable. |
| `Failed to load the scene or camera from: ...` | The parser rejected the scene; check the parser error above. |
| `cannot open shared object file: ./plugins/...` | Run `./raytracer` from the repository root after building. |
