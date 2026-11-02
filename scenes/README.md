# Raytracer test scenes

Each file is a complete scene for the parser. Run one with:

```bash
./raytracer --path scenes/<name>.cfg --sfml
```

Useful options:

- `--frames N` — samples per pixel (higher = cleaner, slower; default 10).
  Use `--frames 30` or more for a clean still image.
- `--output out.ppm` — also write the rendered image as a PPM
- `--width/--height` — size of the SFML window (the render itself is currently 960x540)

While the window is open: `WASD`/`QE` move the camera, `R` resets it,
`X` locks the look-at point, `C` locks the camera position, `Esc` quits.

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
  `planes` (`normal`, `position`, `material`), `cylinders` (`r`, `center`,
  `height`, `material`) and `cones` (`r`, `center` = apex, `h`, `material`).
- **Lights** (`lights`): `ambient` (`intensity`, `color`), `directional`
  (`position` = direction towards the light, `intensity`, `color`) and `point`
  (a list of `position`, `intensity`, `color`, `attenuation`).

Materials are referenced by their index in the `materials` group, starting at 0.
The camera's `rotation` field is the point the camera looks at.
