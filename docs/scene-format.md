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

## Animation

An optional top-level `animation` block declares camera, prefab-instance and
light timelines. The top-level `camera` remains the still-image camera; the
timeline is only played when the CLI asks for it (`--animation`,
`--start-frame`, `--end-frame`, `--fps` or `--video`).

```libconfig
animation:
{
    fps = 24;
    shutter = { angle = 180.0; samples = 8; };

    camera = (
        {
            time = 0.0;
            position = { x = 0.0; y = 2.8; z = -12.5; };
            rotation = { x = 0.0; y = 2.8; z = 1.5; };
            fieldOfView = 48.0;
            lensRadius = 0.03;
            focusDistance = 14.0;
        },
        {
            time = 4.0;
            position = { x = 3.5; y = 4.0; z = -9.0; };
            rotation = { x = 0.0; y = 2.8; z = 1.5; };
            fieldOfView = 52.0;
        }
    );
};
```

- `fps` is an optional positive integer frame rate. It defaults to `24` and can
  be overridden per render with `--fps F`.
- `shutter` is optional and controls motion blur of the camera, animated
  instances and animated lights. `angle` is the shutter
  opening in degrees in `[0, 360)`; `0` or an omitted block disables blur, and
  `180` is a half-frame exposure. `samples` is the number of deterministic
  subframe times and defaults to `1`. Both fields can be overridden with
  `--shutter-angle` and `--shutter-samples`.
- `camera` is a list of at least two keyframes.
- `time` is in seconds and the first keyframe must be exactly `0.0`. Times must
  be non-negative and strictly increasing.
- Every keyframe must produce a valid camera: `position` and `rotation` (the
  look-at point) must differ, and `fieldOfView` must stay inside `(0, 180)`.
- `position`, `rotation`, `fieldOfView`, `lensRadius` and `focusDistance` are
  optional per keyframe. An omitted field inherits the previous keyframe; the
  first keyframe inherits from the top-level `camera`.
- `lensRadius` must be non-negative and `focusDistance` must be positive.
- The animation duration is the time of the last keyframe. The frame count is
  `ceil(duration * fps)` and frames are numbered from `0`.

Position and look-at use Catmull-Rom interpolation across neighbouring
keyframes. Catmull-Rom passes exactly through every keyframe and gives smooth
velocity across segment boundaries; sparse, sharp direction changes can
overshoot, so keep adjacent poses reasonably close. `fieldOfView`,
`lensRadius` and `focusDistance` use linear interpolation. Values requested
outside the timeline are clamped to the first or last keyframe.

If the first keyframe repeats the top-level camera pose and settings, frame `0`
renders identically to the still image. The three animated hero scenes follow
that convention.

Motion blur samples the shutter interval around each frame time, from
`-angle/720` to `+angle/720` of a frame. The subframe offsets are stratified, so
CPU and CUDA render exactly the same poses, and samples outside the timeline are
clamped to the first or last keyframe. The render cost scales with
`shutter.samples`; shutter-off renders are unchanged.

### Scene animation

Named prefab instances and lights can be animated alongside the camera. Add an
`id` to the instance or light definition, then reference it from the animation
block.

```libconfig
primitives:
{
    instances = (
        { prefab = "orrery"; id = "orrery"; transform = { ... }; }
    );
};

lights:
{
    point = (
        { id = "lantern"; position = { ... }; color = { ... }; intensity = 1.0; }
    );
};

animation:
{
    fps = 24;
    camera = ( ... );

    instances = (
        {
            target = "orrery";
            position = (
                { time = 0.0; value = { x = 0.0; y = 0.0; z = 0.0; }; },
                { time = 8.0; value = { x = 0.0; y = 0.4; z = 0.0; }; }
            );
            rotation = (
                { time = 0.0; value = { x = 0.0; y = 0.0; z = 0.0; }; },
                { time = 8.0; value = { x = 0.0; y = 360.0; z = 0.0; }; }
            );
            scale = ( ... ); # optional, inherits the instance base scale
        }
    );

    lights = (
        {
            target = "lantern";
            intensity = (
                { time = 0.0; value = 1.0; },
                { time = 2.0; value = 1.6; }
            );
            color = ( ... ); # optional
            position = ( ... ); # optional, for point and sphere lights
        }
    );
};
```

- `id` is optional; animated targets require one. IDs are unique within the
  instance namespace and within the light namespace.
- Only top-level prefab instances and top-level lights are targetable. Prefab
  internals are cloned and are not separately addressable in this version.
- An instance track requires at least one of `position`, `rotation` or `scale`.
  Omitted channels inherit the instance's base transform.
- A light track requires at least one of `intensity`, `color` or `position`.
  Omitted channels leave the light at its current value.
- Track keyframes start at `0.0`, use strictly increasing times, and must not
  exceed the camera animation duration.
- Translation and scale use Catmull-Rom interpolation. Rotation is interpolated
  with quaternion slerp along the shortest path; the scene format stays in
  Euler degrees. Intensity and color use linear interpolation.
- Scale keyframes must be finite and non-zero. Intensity and color keyframes
  must be finite and non-negative.
- Animated instance transforms replace the instance's base transform for that
  frame. Animated light position applies to point and sphere lights.
- Scene animation is sampled at every shutter subframe, so animated instances
  and lights blur alongside the camera. Without motion blur the timeline is
  applied once per frame.
- The block is validated when the scene loads, so an invalid timeline fails
  even if only a still image is rendered. Still renders ignore scene animation
  and use the base transforms and light values.

The brass orrery, tidal bloom and sakura shrine scenes animate orbit groups,
glass blooms and light intensities respectively. Their first keyframes repeat
the base values, so frame `0` matches the still render.

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

- `texture` is an equirectangular environment map. Paths resolve relative to
  the scene file. `u` wraps around the horizon and `v` is clamped from the
  north to the south pole.
- Supported environment formats are PPM, PNG, JPG and Radiance RGBE `.hdr`.
  LDR images are decoded from sRGB; HDR images keep linear values above `1.0`.
  OpenEXR is not supported yet.
- When a texture is present it takes precedence over the gradient fields.
- `zenith`, `horizon`, and `ground` remain the fallback when no texture is
  given.
- `intensity` must be non-negative and defaults to `1.0`.
- If the block is omitted, the background is black.
- Environment maps are visible in reflections and illuminate diffuse/metallic
  paths through importance-sampled path tracing.
- Backgrounds are top-level only; nested scenes inherit the parent background.

`scenes/environment.cfg` demonstrates an equirectangular sky used for the
background, reflections and path-traced lighting.

## Atmosphere

An optional top-level `atmosphere` block adds a participating medium. It is
integrated by both the CPU and CUDA path tracers, including shadow-ray and
environment transmittance.

Homogeneous fog:

```libconfig
atmosphere:
{
    type = "homogeneous";
    density = 0.025;
    color = { r = 0.85; g = 0.90; b = 1.0; };
    anisotropy = 0.35;
    maxDistance = 80.0;
};
```

Height fog:

```libconfig
atmosphere:
{
    type = "height";
    density = 0.035;
    color = { r = 0.75; g = 0.80; b = 1.0; };
    anisotropy = 0.25;
    groundY = 0.0;
    heightFalloff = 0.18;
    maxDistance = 80.0;
};
```

- `density` is the extinction coefficient and must be non-negative. A density
  of `0` leaves rendering unchanged.
- `color` is the scattering albedo and must lie in `[0, 1]` per channel.
- `anisotropy` is the Henyey-Greenstein `g` parameter and must be strictly
  between `-1` and `1`. Positive values scatter forward and produce stronger
  light shafts; `0` is isotropic.
- `maxDistance` clips medium integration for rays that leave the scene and must
  be positive. It defaults to `100`.
- Height fog density is `density * exp(-heightFalloff * (y - groundY))`.
  `groundY` and a positive `heightFalloff` are required.
- Scattering distances are sampled analytically from the medium's optical
  depth, so homogeneous fog needs no ray marching.
- Direct lights, environment lighting and background radiance are attenuated
  by transmittance. Ambient light uses a `maxDistance` attenuation
  approximation.
- Dielectrics, emissives and metals still interact with the medium on every
  path segment.
- The atmosphere is not animated; scene animation changes geometry and lights,
  not density.

`scenes/misty_shrine.cfg`, `scenes/volumetric_cathedral.cfg` and
`scenes/lagoon_mist.cfg` demonstrate height fog, god rays and thin water haze.

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
The path tracer tracks one inside/outside state per ray, so overlapping
dielectric objects with different IORs are approximated.

### Textures

Any material can reference an albedo texture:

```libconfig
name = {
    type = "metallic";
    roughness = 0.35;
    color = { r = 0.9; g = 0.7; b = 0.4; };
    texture = "textures/checker.ppm";
    roughnessTexture = "textures/roughness.ppm";
    normalTexture = "textures/normal.ppm";
    emissionTexture = "textures/emissive.ppm";
};
```

- `texture` is an albedo map and multiplies the material colour.
- `roughnessTexture` modulates the material roughness. The red channel is used
  and the value multiplies the scalar `roughness`; the result is clamped to
  `[0, 1]`.
- `emissionTexture` multiplies the material emission colour. It is useful for
  emissive signs, windows and glowing panels.
- `normalTexture` is a tangent-space RGB normal map. RGB values decode to
  `[-1, 1]`; the perturbed normal is used for shading and bounces.
- Paths resolve relative to the scene file, like meshes.
- Supported formats: binary `P6` or ASCII `P3` PPM, PNG, JPG and Radiance
  RGBE `.hdr`. `P6` files must use an 8-bit maximum value.
- Sampling is bilinear with repeat wrapping.
- Albedo and emission maps are treated as sRGB and decoded to linear. Roughness
  and normal maps are loaded as linear data. HDR textures keep their linear
  values.
- Normal maps are evaluated in a UV-aligned tangent frame. Meshes use a
  per-triangle tangent derived from the corresponding OBJ `vt` values, and
  analytic primitives use their natural U tangent. A deterministic fallback
  frame is used only when the tangent is degenerate (for example a mesh
  without texture coordinates). Mirrored UV islands currently assume a
  right-handed tangent frame.

UV conventions:

| Primitive | `u` | `v` |
| --- | --- | --- |
| Sphere | longitude, wrapping | `0` at +Y, `1` at -Y |
| Plane | local X | local Z |
| Cylinder | angle around the axis | normalized height |
| Cone | angle around the Y axis | normalized apex-to-base height |
| Box | normalized across the hit face | normalized across the hit face |
| Disk | tangent-plane X | tangent-plane Y |
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

### Boxes

```libconfig
boxes = (
    {
        material = 0;
        min = { x = -1.0; y = 0.0; z = -1.0; };
        max = { x = 1.0; y = 2.0; z = 1.0; };
        transform = { translate = { x = 3.0; y = 0.0; z = 0.0; }; };
    }
);
```

- `min` and `max` are the local-space corners of an axis-aligned box.
- Every component of `min` must be strictly below the corresponding `max`.
- Use `transform` to translate, rotate or scale the box.
- UVs are normalized across the hit face.

### Disks and rings

```libconfig
disks = (
    {
        material = 1;
        center = { x = 0.0; y = 1.5; z = 0.0; };
        normal = { x = 0.0; y = 1.0; z = 0.0; };
        radius = 1.2;
        innerRadius = 0.4;
        transform = { rotate = { x = 15.0; y = 0.0; z = 0.0; }; };
    }
);
```

- `normal` is the disk plane normal and must be non-zero.
- `radius` must be positive; `innerRadius` is optional, non-negative and must be
  smaller than `radius`. An inner radius creates a ring.
- Use `transform` to orient and place the disk.

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
Ke 0.35 0.35 0.35
map_Kd ../textures/checker.ppm
map_Ks ../textures/roughness.ppm
map_Ke ../textures/emission.ppm
```

- `Kd` is the diffuse colour.
- `Ka` and `Ks` contribute ambient and specular reflection coefficients.
- `Ns` is converted to a roughness value.
- `Ke` is applied as a constant additive emission colour.
- `map_Kd` is an albedo texture resolved relative to the MTL file; the same
  PPM/PNG/JPG/HDR formats are supported.
- `map_Ks` is imported as a roughness map. It is an approximation: the red
  channel multiplies the `Ns`-derived roughness, because MTL specular maps are
  not physically equivalent to microfacet roughness maps. Author maps with
  bright values for rougher surfaces.
- `map_Ke` is imported as an emission map and multiplies `Ke`. If `Ke` is
  absent, a white emission colour is used so the map alone drives the glow.
- `d` and `Tr` are accepted but not fully applied yet.
- Map options such as `-s`/`-o` are not supported.

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
        transform = {
            translate = { x = 0.0; y = 0.8; z = 0.0; };
            rotate = { x = 0.0; y = 30.0; z = 0.0; };
            scale = 1.15;
        };
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
- nested lights are added to the parent lights,
- an optional `transform` moves primitives and lights as one group.

A nested scene should define its own materials. Its material indices are local
to that nested scene before merging.

Group transforms use the same translation, rotation and scale rules as
primitive transforms. Primitives are wrapped in an instance transform; lights
are transformed in place:

- point lights have their position transformed,
- directional lights have their direction rotated and normalised,
- sphere area lights have their centre transformed and radius scaled by the
  largest basis length (conservative under non-uniform scale),
- ambient lights are unaffected.

`scenes/group_transform.cfg` moves three spheres and a point light together.

### Prefabs

Top-level `prefabs` define reusable scene fragments. Each prefab may contain
`materials`, `primitives` and `lights`, but no camera:

```libconfig
prefabs:
{
    lamp = {
        materials = { /* ... */ };
        primitives = { /* ... */ };
        lights = { /* ... */ };
    };
};
```

Instantiate prefabs through `primitives.instances`, with an optional
`transform`:

```libconfig
primitives:
{
    instances = (
        {
            prefab = "lamp";
            transform = { translate = { x = -2.8; y = 0.0; z = 0.4; }; };
        },
        {
            prefab = "lamp";
            transform = {
                translate = { x = 2.8; y = 0.0; z = 0.0; };
                rotate = { x = 0.0; y = 25.0; z = 0.0; };
                scale = 1.1;
            };
        }
    );
};
```

Each instance copies the prefab's materials, primitives and lights, then
applies the instance transform. Material indices are adjusted independently
per instance, so repeated instantiations do not share mutable state.

- Prefabs must be defined at the top level.
- Prefabs may contain nested scenes, but **not** prefab instances. Referencing
  a prefab from inside another prefab is rejected to rule out recursion.
- Unknown prefab names and duplicate prefab definitions are parser errors.
- Prefabs are parsed before primitives, so order inside the file is not
  significant for top-level definitions.

`scenes/prefabs.cfg` instantiates one lamp prefab three times.

### Scatter

`primitives.scatter` generates deterministic prefab instances inside an
axis-aligned area:

```libconfig
primitives:
{
    scatter = (
        {
            prefab = "spore";
            count = 180;
            seed = 17;
            area = {
                min = { x = -5.0; y = 0.2; z = -1.0; };
                max = { x = 5.0; y = 3.6; z = 8.0; };
            };
            rotation = {
                min = { x = 0.0; y = 0.0; z = 0.0; };
                max = { x = 0.0; y = 360.0; z = 0.0; };
            };
            scale = { min = 0.6; max = 1.4; };
        }
    );
};
```

- `count` must be between `1` and `4096`.
- `seed` drives a deterministic random sequence.
- `area.min`/`area.max` define the placement box; each component of `min` must
  not exceed `max`.
- Optional `rotation.min`/`rotation.max` are Euler-degree ranges.
- Optional `scale.min`/`scale.max` are positive uniform scale ranges.
- Scatter definitions inside prefabs are rejected.
- Scatter instances are not individually addressable by scene animation in
  this version.

`scenes/scatter_field.cfg` demonstrates 180 emissive spores.

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

Sphere lights are sampled as area sources using a cone around the light centre.
Two random values choose a direction uniformly inside the sphere's subtended
solid angle, and the returned radiance is scaled by that solid angle. This
produces physically consistent soft shadows. Because the control is
radiance-like rather than a point-light intensity, sphere-light intensities in
the sample scenes are larger than nearby point-light intensities. Larger radii
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
- unreadable or malformed PPM/PNG/JPG/HDR textures,
- non-finite point, vector, or colour components, or values that overflow the
  renderer's float representation,
- field of view outside `(0, 180)`,
- a negative camera lens radius or a non-positive focus distance,
- a camera position equal to its look-at point,
- an animation fps that is not a positive integer,
- a shutter angle outside `[0, 360)` or a non-positive shutter sample count,
- an animation with fewer than two keyframes, a first keyframe not at time
  zero, or non-increasing keyframe times,
- duplicate or unknown scene-animation targets, tracks with no channels,
  zero scale keyframes, or track times beyond the animation duration,
- box corners where `min` is not strictly below `max`,
- disk radii that are non-positive, or an inner radius outside
  `[0, radius)`,
- scatter counts outside `1..4096`, inverted area/rotation ranges, or
  non-positive scale ranges,
- unknown atmosphere types, negative density, colour outside `[0, 1]`,
  anisotropy outside `(-1, 1)`, non-positive `maxDistance`, or height fog
  without `groundY`/positive `heightFalloff`,
- negative material indices, or indices outside the material list.

Renderer selection errors (unknown renderer names, missing plugins, unavailable
CUDA devices) are handled at runtime and are documented in [cli.md](cli.md).

## Output

The renderer writes binary `P6` PPM files or 8-bit RGBA PNG files, selected by
the output extension (`.ppm` or `.png`). The provided render scripts create 4K
and 1080p previews. Animated renders write one file per frame, or can be encoded
directly to MP4/WebM with `--video`; see [cli.md](cli.md) for frame patterns,
ranges and video options.
