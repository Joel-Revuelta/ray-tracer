# Scene format

Scenes are libconfig files. The parser is strict: a missing or mistyped setting,
an invalid camera, a non-positive primitive dimension, a zero normal/direction,
or an out-of-range material index aborts the load before rendering.

## Minimal scene

```libconfig
camera:
{
    position = { x = 0.0; y = 1.0; z = -5.0; };
    rotation = { x = 0.0; y = 0.0; z = 0.0; };
    fieldOfView = 60.0;
};

materials:
{
    red = {
        type = "diffuse";
        ambientReflection = 0.25;
        diffuseReflection = 0.9;
        color = { r = 0.9; g = 0.1; b = 0.1; };
    };
};

primitives:
{
    spheres = (
        {
            material = 0;
            r = 1.0;
            position = { x = 0.0; y = 0.0; z = 0.0; };
        }
    );
};

lights:
{
    ambient = {
        intensity = 0.4;
        color = { r = 1.0; g = 1.0; b = 1.0; };
    };
};
```

## Camera

```libconfig
camera:
{
    position = { x = 0.0; y = 1.0; z = -5.0; };
    rotation = { x = 0.0; y = 0.0; z = 0.0; };
    fieldOfView = 60.0;
};
```

- `position` is the camera eye point.
- `rotation` is the point the camera looks at (not Euler angles).
- `fieldOfView` is the vertical field of view in degrees. It must be strictly
  greater than `0` and strictly less than `180`.
- `position` and `rotation` must differ. A coincident camera is rejected.
- The render resolution is set by the CLI (`--width`/`--height`), not by the
  scene file.

## Background

An optional gradient background is returned when a camera or bounce ray misses
all geometry:

```libconfig
background:
{
    zenith = { r = 0.02; g = 0.04; b = 0.12; };
    horizon = { r = 0.15; g = 0.20; b = 0.38; };
    ground = { r = 0.01; g = 0.01; b = 0.03; };
    intensity = 1.0;
};
```

- `zenith`, `horizon`, and `ground` define the vertical gradient.
- `intensity` must be non-negative and defaults to `1.0`.
- If the block is omitted, the background is black.
- Backgrounds are top-level only; nested scenes inherit the parent background.

## Materials

Materials are declared in order and referenced by their zero-based index in the
`materials` group.

### Diffuse

```libconfig
name = {
    type = "diffuse";
    ambientReflection = 0.25;
    diffuseReflection = 0.90;
    color = { r = 0.8; g = 0.2; b = 0.2; };
};
```

### Metallic

```libconfig
name = {
    type = "metallic";
    color = { r = 0.9; g = 0.9; b = 1.0; };
    roughness = 0.12;
};
```

`roughness` is optional and must be in `[0, 1]`; it defaults to `0.0`.
`0.0` is a perfect mirror; higher values produce a GGX microfacet blur with a
Schlick Fresnel response.

The legacy spelling `metalic` is accepted as an alias for `metallic`.

### Emissive

```libconfig
name = {
    type = "emissive";
    intensity = 1.5;
    color = { r = 1.0; g = 0.5; b = 0.1; };
    emissionColor = { r = 1.0; g = 0.4; b = 0.05; };
};
```

`emissionColor` is scaled by `intensity` when emission is accumulated.

### Flat colour

```libconfig
name = {
    type = "flatColor";
    color = { r = 0.8; g = 0.1; b = 0.4; };
};
```

Flat surfaces receive lighting for the first hit but do not bounce another ray.

### Dielectric

```libconfig
name = {
    type = "dielectric";
    ior = 1.5;
    color = { r = 1.0; g = 1.0; b = 1.0; };
};
```

`ior` is the refractive index and must be positive; it defaults to `1.5`.
`color` tints the transmitted path. Dielectrics use Fresnel reflection,
refraction, and total internal reflection instead of diffuse lighting.

## Primitives

All primitives require a `material` index.

### Spheres

```libconfig
spheres = (
    {
        material = 0;
        r = 1.0;
        position = { x = 0.0; y = 0.0; z = 0.0; };
    }
);
```

`r` must be positive.

### Planes

```libconfig
planes = (
    {
        material = 1;
        position = { x = 0.0; y = 0.0; z = 0.0; };
        normal = { x = 0.0; y = 1.0; z = 0.0; };
    }
);
```

The normal must be non-zero. It is normalized internally.

### Cylinders

```libconfig
cylinders = (
    {
        material = 2;
        r = 1.0;
        center = { x = 0.0; y = 0.0; z = 0.0; };
        height = 2.0;
    }
);
```

- `center` is the centre of the **bottom cap**.
- `height` extends upward along the default Y axis.
- `r` and `height` must be positive.
- Cylinder rotation/axis changes are available through the transformation
  plugin API, but are not exposed by the current scene parser.

### Cones

```libconfig
cones = (
    {
        material = 3;
        r = 1.0;
        center = { x = 0.0; y = 2.0; z = 0.0; };
        h = 2.0;
    }
);
```

- `center` is the **apex**.
- The base is `h` units below the apex.
- `r` and `h` must be positive.
- Cone rotation is available through the transformation API, not the current
  scene parser.

### Nested scenes

A `scene` block inside `primitives` is merged into the parent scene:

```libconfig
primitives:
{
    spheres = ( /* parent spheres */ );

    scene = {
        materials = { /* nested materials */ };
        primitives = { /* nested primitives */ };
        lights = { /* nested lights */ };
    };
};
```

When merged:

- nested materials are appended to the parent material list,
- nested primitive material indices are shifted to point at the appended
  materials,
- nested lights are added to the parent lights.

A nested scene should define its own materials. Its material indices are local
to that nested scene before merging.

## Lights

### Ambient

```libconfig
lights:
{
    ambient = {
        intensity = 0.2;
        color = { r = 0.4; g = 0.5; b = 0.9; };
    };
};
```

`intensity` must be non-negative.

### Directional

```libconfig
directional = {
    position = { x = -1.0; y = 1.5; z = -0.8; };
    intensity = 0.7;
    color = { r = 1.0; g = 0.95; b = 0.9; };
};
```

`position` is the direction **towards the light**, not a world position. It must
be non-zero. `intensity` must be non-negative.

### Point

```libconfig
point = (
    {
        position = { x = -4.0; y = 3.0; z = -2.0; };
        intensity = 1.2;
        color = { r = 1.0; g = 0.5; b = 0.2; };
        attenuation = {
            constant = 1.0;
            linear = 0.02;
            quadratic = 0.001;
        };
    }
);
```

- `intensity` must be non-negative.
- `attenuation.constant` must be greater than zero.
- `attenuation.linear` and `attenuation.quadratic` must be non-negative.
- `point` may also be a single object instead of a list.

Attenuation is:

```text
intensity / (constant + linear * distance + quadratic * distance^2)
```

## Material indices

Material indices are positional:

```libconfig
materials:
{
    first = { ... };   # index 0
    second = { ... };  # index 1
};
```

A primitive with `material = 1` uses `second`. Indices must be in range or the
scene is rejected.

## Coordinate conventions

- Y is up in the sample scenes.
- The camera is placed at `position` and looks towards `rotation`.
- The plane `normal` is normalized internally and flipped to face the incoming
  ray for lighting.
- Cylinders extend from `center` along +Y by `height`.
- Cones use `center` as the apex and extend `h` units down to the base.

## Validation summary

The parser rejects:

- missing settings and wrong libconfig types,
- unknown material types,
- sphere/cylinder/cone radius or height `<= 0`,
- zero plane normals,
- zero directional-light directions,
- non-positive point-light attenuation constants,
- negative intensities and negative linear/quadratic attenuation,
- negative diffuse material reflection coefficients or emissive intensity,
- non-positive dielectric IOR,
- negative background intensity,
- metallic roughness outside `[0, 1]`,
- non-finite point, vector, or colour components,
- field of view outside `(0, 180)`,
- a camera position equal to its look-at point,
- negative material indices, or indices outside the material list.

Renderer selection errors (unknown renderer names, missing plugins, unavailable
CUDA devices) are handled at runtime and are documented in [cli.md](cli.md).

## Output

The renderer writes binary `P6` PPM files. Use Pillow or ImageMagick to convert
them to PNG. The provided render scripts also create 4K and 1080p previews.
