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
- `lensRadius` is optional and sets the camera's lens disk radius in world
  units. `0` (the default) is a pinhole camera.
- `focusDistance` is optional and sets the distance from the eye to the focus
  plane. It defaults to the look-at distance and must be positive.
- `position` and `rotation` must differ. A coincident camera is rejected.
- The render resolution is set by the CLI (`--width`/`--height`), not by the
  scene file.

With a non-zero `lensRadius`, each camera ray starts at a random point on the
lens disk and is aimed at the focus plane. Geometry at `focusDistance` stays
sharp; nearer and farther geometry blurs. This is a Monte Carlo effect, so use
more samples for clean out-of-focus highlights.

```libconfig
camera:
{
    position = { x = 0.0; y = 1.8; z = -14.0; };
    rotation = { x = 0.0; y = 0.2; z = 0.0; };
    fieldOfView = 48.0;
    lensRadius = 0.35;
    focusDistance = 12.0;
};
```

## Background

An optional background is returned when a camera or bounce ray misses all
geometry. It can be a vertical gradient or an equirectangular environment map.

Gradient:

```libconfig
background:
{
    zenith = { r = 0.02; g = 0.04; b = 0.12; };
    horizon = { r = 0.15; g = 0.20; b = 0.38; };
    ground = { r = 0.01; g = 0.01; b = 0.03; };
    intensity = 1.0;
};
```

Environment map:

```libconfig
background:
{
    texture = "textures/sky.ppm";
    intensity = 1.0;
};
```

- `texture` is an equirectangular PPM environment map. Paths resolve relative
  to the scene file. `u` wraps around the horizon and `v` is clamped from the
  north to the south pole.
- When a texture is present it takes precedence over the gradient fields.
- `zenith`, `horizon`, and `ground` remain the fallback when no texture is
  given.
- `intensity` must be non-negative and defaults to `1.0`.
- If the block is omitted, the background is black.
- Environment maps are visible in reflections and illuminate diffuse/metallic
  paths through ordinary path tracing. They are LDR PPM images; HDR/EXR
  environments are not supported.
- Backgrounds are top-level only; nested scenes inherit the parent background.

`scenes/environment.cfg` demonstrates an equirectangular sky used for the
background, reflections and path-traced lighting.

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

### Textures

Any material can reference an albedo texture:

```libconfig
name = {
    type = "diffuse";
    ambientReflection = 0.25;
    diffuseReflection = 0.90;
    color = { r = 1.0; g = 1.0; b = 1.0; };
    texture = "textures/checker.ppm";
};
```

- Paths resolve relative to the scene file, like meshes.
- Supported format: binary `P6` or ASCII `P3` PPM. `P6` files must use an
  8-bit maximum value.
- Sampling is bilinear with repeat wrapping.
- The sampled texel multiplies the material's `color`; a white tint leaves the
  texture unchanged.
- Textures currently affect albedo only. Emission, roughness and normal maps
  are not supported.

UV conventions:

| Primitive | `u` | `v` |
| --- | --- | --- |
| Sphere | longitude, wrapping | `0` at +Y, `1` at -Y |
| Plane | local X | local Z |
| Cylinder | angle around the axis | normalized height |
| Cone | angle around the Y axis | normalized apex-to-base height |
| Mesh | OBJ `vt` values, interpolated per triangle |

`scenes/textures.cfg` demonstrates a checker texture on a plane, sphere,
cylinder, cone and mesh.

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
- Use a `transform` block to tilt or scale a cylinder (see
  [Transforms](#transforms)).

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
- Use a `transform` block to tilt or scale a cone (see
  [Transforms](#transforms)).

### Meshes

Meshes are loaded from Wavefront OBJ files and declared in a `meshes` list:

```libconfig
primitives:
{
    meshes = (
        {
            material = 0;
            path = "models/icosphere.obj";
            transform = {
                translate = { x = -2.0; y = 0.0; z = 0.0; };
                scale = 1.5;
            };
        },
        {
            material = 1;
            path = "models/multi_material.obj";
            materialGroups = {
                red = 2;
                glass = 3;
            };
            transform = {
                translate = { x = 2.0; y = 0.5; z = 0.0; };
                rotate = { x = 0.0; y = 35.0; z = 0.0; };
            };
        }
    );
};
```

- `path` is required and resolves relative to the scene file's directory.
  Absolute paths are also accepted.
- `material` is required and is the fallback for faces without a mapped group.
- `materialGroups` optionally maps OBJ `usemtl` names to zero-based material
  indices. Indices must exist in the current scene.
- If the OBJ references `mtllib` files, their materials are loaded
  automatically and matched to `usemtl` names. Explicit `materialGroups`
  entries override the automatic match.
- Groups that are not listed or not present in an MTL, and faces before the
  first `usemtl`, use the mesh's `material` fallback.
- `transform` is optional and follows the same rules as
  [Transforms](#transforms).
- Repeating the same resolved path shares one immutable mesh asset. Each entry
  still gets its own material fallback, group map and transform.
- The loader supports `v` positions, `vt` texture coordinates, `vn` vertex
  normals, `f` faces, `usemtl` groups and `mtllib` references. Polygons are fan
  triangulated and negative indices are supported. Objects and comments are
  ignored.
- Face tokens may use `v`, `v/vt`, `v//vn` or `v/vt/vn`. A face without normals
  falls back to the geometric face normal.
- When `vn` data is present, normals are interpolated across the triangle for
  smooth shading. Without `vn`, the mesh is flat-shaded.
- Shading normals drive diffuse/metallic lighting and sampling. Geometric
  normals are still used for ray offsets and dielectric reflection/refraction.
- Triangles are double-sided.
- The parser rejects unreadable files, malformed numbers, face indices out of
  range, faces with fewer than three vertices, empty meshes and meshes above
  the internal triangle limit.

The sample assets live in `scenes/models/`; `scenes/meshes.cfg` demonstrates
shared geometry and per-instance transforms, `scenes/obj_materials.cfg`
demonstrates explicit per-face material groups, and `scenes/mtl_materials.cfg`
demonstrates automatic MTL import.

### MTL materials

An OBJ can reference one or more material libraries:

```text
mtllib materials.mtl
```

MTL paths resolve relative to the OBJ file. The parser supports:

```text
newmtl painted
Ka 0.20 0.20 0.20
Kd 0.85 0.12 0.12
Ks 0.35 0.35 0.35
Ns 64
map_Kd ../textures/checker.ppm
```

- `Kd` is the diffuse colour.
- `Ka` and `Ks` contribute ambient and specular reflection coefficients.
- `Ns` is converted to a roughness value.
- `map_Kd` is an albedo texture resolved relative to the MTL file.
- `Ke`, `d`, `Tr` and unsupported maps are accepted but not fully applied yet.
- `map_Kd` options such as `-s`/`-o` are not supported.

MTL materials are appended to the scene automatically. `usemtl` names match
MTL material names; explicit `materialGroups` entries still take precedence.
Unmatched groups use the mesh's `material` fallback.

### Transforms

Every primitive accepts an optional `transform` group. It is applied to the
primitive's local coordinates before the world-space bounding box is built:

```libconfig
spheres = (
    {
        material = 0;
        r = 1.0;
        position = { x = 0.0; y = 0.0; z = 0.0; };
        transform = {
            translate = { x = 2.0; y = 1.0; z = 0.0; };
            rotate = { x = 0.0; y = 30.0; z = 0.0; };
            scale = { x = 1.5; y = 0.75; z = 1.0; };
        };
    }
);
```

- `translate` is a vector in world units, default `{ 0, 0, 0 }`.
- `rotate` is an XYZ Euler rotation in **degrees**, default `{ 0, 0, 0 }`.
  Rotations are applied in X, then Y, then Z order.
- `scale` is either a single number or a vector, default `1.0`. A scalar
  applies uniformly; a vector enables ellipsoids and stretched cylinders.
- The composed transform is `world = T * Rz * Ry * Rx * S * local`.
- Scale components must be non-zero and the resulting matrix must be
  invertible.
- Transforms are applied around the world origin, not around the primitive's
  own centre. To rotate an object in place, define it at the origin and use
  `translate` to position it.
- Planes remain unbounded for acceleration purposes even when transformed.

`scenes/transforms.cfg` demonstrates translated, rotated and non-uniformly
scaled spheres, cylinders, cones and planes.

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

### Sphere (area light)

```libconfig
spheres = (
    {
        position = { x = -3.8; y = 4.0; z = -3.0; };
        radius = 1.1;
        color = { r = 1.0; g = 0.70; b = 0.45; };
        intensity = 3.6;
        attenuation = {
            constant = 1.0;
            linear = 0.05;
            quadratic = 0.003;
        };
    }
);
```

- `position` is the centre of the sphere light.
- `radius` must be positive.
- `intensity` must be non-negative.
- `attenuation` is optional and defaults to `constant = 1`, `linear = 0`,
  `quadratic = 0`. The same rules and formula as point lights apply.
- `spheres` may also be a single object instead of a list.

Sphere lights are sampled as area sources. Each shading query picks a point on
the sphere and casts a shadow ray to it, producing soft shadows. Larger radii
give wider penumbrae, and soft shadows need more samples to converge cleanly.
`scenes/soft_shadows.cfg` demonstrates two sphere lights over a floor.

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
- Primitive transforms are applied around the world origin, before the
  primitive is placed in the scene.

## Validation summary

The parser rejects:

- missing settings and wrong libconfig types,
- unknown material types,
- sphere/cylinder/cone radius or height `<= 0`,
- zero plane normals,
- zero directional-light directions,
- non-positive point-light or sphere-light attenuation constants,
- non-positive sphere-light radii,
- negative intensities and negative linear/quadratic attenuation,
- negative diffuse material reflection coefficients or emissive intensity,
- non-positive dielectric IOR,
- negative background intensity,
- metallic roughness outside `[0, 1]`,
- transform scale components equal to zero or non-invertible transforms,
- unreadable or malformed OBJ/MTL files, out-of-range mesh position/normal/
  texture indices, empty `usemtl`/`mtllib` names, zero-length normals, or empty
  meshes,
- material-group indices outside the current scene's material list,
- unreadable or malformed PPM textures,
- non-finite point, vector, or colour components,
- field of view outside `(0, 180)`,
- a negative camera lens radius or a non-positive focus distance,
- a camera position equal to its look-at point,
- negative material indices, or indices outside the material list.

Renderer selection errors (unknown renderer names, missing plugins, unavailable
CUDA devices) are handled at runtime and are documented in [cli.md](cli.md).

## Output

The renderer writes binary `P6` PPM files. Use Pillow or ImageMagick to convert
them to PNG. The provided render scripts also create 4K and 1080p previews.
