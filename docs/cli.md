# CLI reference

The `raytracer` executable parses a libconfig scene, selects a renderer plugin
and either renders headlessly or opens the SFML window.

## Synopsis

```text
./raytracer --path <scene.cfg> --cli|--sfml [options]
```

At least one of `--cli` or `--sfml` is required. `--help` and
`--list-renderers` work without a scene.

## Options

| Option | Argument | Description | Default |
| --- | --- | --- | --- |
| `--path` | scene file | Scene configuration to load. | required |
| `--cli` | — | Headless rendering. | |
| `--sfml` | — | Interactive SFML window. | |
| `--renderer` | `cpu` or `cuda` | Renderer plugin name. Case-insensitive. | `cpu` |
| `--list-renderers` | — | Print renderer availability and exit `0`. | |
| `--width` | positive integer | Render width, window width and PPM width. | `960` |
| `--height` | positive integer | Render height, window height and PPM height. | `540` |
| `--frames` | positive integer | Samples per pixel. | `10` |
| `--output` | path | PPM output file. | stdout in CLI mode |
| `--help`, `-h` | — | Print usage and exit `0`. | |

### Samples, not animation frames

`--frames N` means **N samples per pixel**. Each sample traces a camera ray and
up to 15 bounces. More samples reduce Monte Carlo noise but scale render time
roughly linearly.

### Output

- PPM values are gamma-corrected from the renderer's linear radiance.
- CLI mode with `--output` writes a binary `P6` PPM file.
- CLI mode without `--output` writes the PPM to standard output.
- SFML mode writes a PPM only when `--output` is provided.
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
| `Esc` | Close the window. |

Movement is camera-relative: `W`/`S` follow the horizontal view direction and
`A`/`D` follow the camera's right vector. `Q`/`E` remain world-space vertical
movement. `X` keeps the look-at point fixed while the camera moves; `C` keeps the
camera position fixed while the look target moves.

Resizing the window stretches the rendered image to the new window size. The
CUDA renderer updates progressively and processes input continuously while a
sample is rendering.

For 8K or very large renders, use `--cli`; opening a 7680×4320 SFML window is not
practical on most displays.

## Examples

### CPU preview

```bash
./raytracer --path scenes/showcase.cfg --cli \
  --renderer cpu --width 960 --height 540 --frames 30 \
  --output preview.ppm
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

## Common errors

| Message | Resolution |
| --- | --- |
| `Unknown or incomplete option.` | Check the option spelling and required argument. |
| `Invalid width/height/frames value` | Values must be positive integers. |
| `Unknown renderer: <name>` | Use `--list-renderers` to see valid names. |
| `Renderer '<name>' is unavailable: ...` | The plugin is missing or the runtime/device is unavailable. |
| `Cannot open output file: ...` | Ensure the parent directory exists and is writable. |
| `Failed to load the scene or camera from: ...` | The parser rejected the scene; check the parser error above. |
| `cannot open shared object file: ./plugins/...` | Run `./raytracer` from the repository root after building. |
