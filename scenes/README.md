# Raytracer test scenes

Each file is a complete scene for the parser. Run one interactively with:

```bash
./raytracer --path scenes/<name>.cfg --sfml
```

Or render it headlessly without a display server:

```bash
./raytracer --path scenes/<name>.cfg --cli --width 960 --height 540 --frames 30 --output render.ppm
```

Useful options:

- `--cli` — render without opening a window. The PPM image is written to
  `--output`, or to standard output when `--output` is omitted.
- `--sfml` — open the interactive SFML window. A PPM is only written when
  `--output` is given.
- `--frames N` — samples per pixel (higher = cleaner, slower; default 10).
  Use `--frames 30` or more for a clean still image.
- `--output out.ppm` — write the rendered image as a PPM.
- `--width/--height` — render resolution, SFML window size and PPM size
  (default 960x540).

While the window is open: `WASD`/`QE` move the camera, `R` resets it,
`X` locks the look-at point, `C` locks the camera position, `Esc` quits.
Resizing the window stretches the rendered image to the new size.

| Scene | What it shows |
| --- | --- |
| `showcase.cfg` | Diffuse, metallic, emissive and flat materials together, three light types |
| `primitives.cfg` | Spheres, a cylinder, a cone and an infinite plane |
| `materials.cfg` | The four material models side by side on identical spheres |
| `reflections.cfg` | A metallic mirror floor with coloured metallic spheres |
| `lighting.cfg` | Red, green and blue point lights plus visible emissive bulbs |
| `orbits.cfg` | A small planetary scene with an emissive sun, planets and a starfield |

## Scene format

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

The parser is strict: a missing or mistyped setting, an unknown material type
or an out-of-range material index prints an error and the program exits with a
non-zero status without writing an image.
