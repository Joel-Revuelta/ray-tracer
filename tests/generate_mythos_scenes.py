#!/usr/bin/env python3
"""Generate the MYTHOS scene pack from a small libconfig builder."""

from __future__ import annotations

import math
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
SCENES = ROOT / "scenes"


def f(value: float) -> str:
    text = f"{value:.4f}".rstrip("0").rstrip(".")
    return text if "." in text else text + ".0"


def vec(x: float, y: float, z: float) -> str:
    return f"{{ x = {f(x)}; y = {f(y)}; z = {f(z)}; }}"


def color(r: float, g: float, b: float) -> str:
    return f"{{ r = {f(r)}; g = {f(g)}; b = {f(b)}; }}"


def camera(position: tuple[float, float, float], look_at: tuple[float, float, float],
           field_of_view: float, lens_radius: float = 0.0, focus_distance: float = 10.0) -> str:
    return f'''camera:
{{
    position = {vec(*position)};
    rotation = {vec(*look_at)};
    fieldOfView = {f(field_of_view)};
    lensRadius = {f(lens_radius)};
    focusDistance = {f(focus_distance)};
}};
'''


def material(name: str, kind: str, body: str) -> str:
    return f'''    {name} = {{
        type = "{kind}";
{body}
    }};'''


def diffuse(name: str, colour: tuple[float, float, float], ambient: float = 0.18,
            diffuse_reflection: float = 0.82, texture: str | None = None) -> str:
    body = f'''        ambientReflection = {f(ambient)};
        diffuseReflection = {f(diffuse_reflection)};
        color = {color(*colour)};'''
    if texture:
        body += f'\n        texture = "{texture}";'
    return material(name, "diffuse", body)


def metallic(name: str, colour: tuple[float, float, float], roughness: float) -> str:
    return material(name, "metallic", f'''        color = {color(*colour)};
        roughness = {f(roughness)};''')


def dielectric(name: str, colour: tuple[float, float, float], ior: float) -> str:
    return material(name, "dielectric", f'''        ior = {f(ior)};
        color = {color(*colour)};''')


def emissive(name: str, intensity: float, colour: tuple[float, float, float],
             emission: tuple[float, float, float]) -> str:
    return material(name, "emissive", f'''        intensity = {f(intensity)};
        color = {color(*colour)};
        emissionColor = {color(*emission)};''')


def point_light(position: tuple[float, float, float], colour: tuple[float, float, float],
                intensity: float, linear: float = 0.04, quadratic: float = 0.002) -> str:
    return f'''        point = {{
            position = {vec(*position)};
            color = {color(*colour)};
            intensity = {f(intensity)};
            attenuation = {{ constant = 1.0; linear = {f(linear)}; quadratic = {f(quadratic)}; }};
        }};'''


def sphere_light(position: tuple[float, float, float], radius: float,
                 colour: tuple[float, float, float], intensity: float,
                 ident: str | None = None) -> str:
    id_line = f'\n            id = "{ident}";' if ident else ""
    return f'''        {{
            position = {vec(*position)};{id_line}
            radius = {f(radius)};
            color = {color(*colour)};
            intensity = {f(intensity)};
            attenuation = {{ constant = 1.0; linear = 0.03; quadratic = 0.002; }};
        }}'''


def sphere(material_index: int, radius: float, position: tuple[float, float, float],
           transform: str | None = None) -> str:
    block = f'''            {{ material = {material_index}; r = {f(radius)}; position = {vec(*position)};'''
    if transform:
        block += f"\n              transform = {{ {transform} }};"
    return block + " }"


def mesh(material_index: int, path: str, transform: str) -> str:
    return f'''            {{
                material = {material_index};
                path = "{path}";
                transform = {{ {transform} }};
            }}'''


def transform(translate: tuple[float, float, float] | None = None,
              rotate: tuple[float, float, float] | None = None,
              scale: tuple[float, float, float] | float | None = None) -> str:
    parts = []
    if translate:
        parts.append(f"translate = {vec(*translate)};")
    if rotate:
        parts.append(f"rotate = {vec(*rotate)};")
    if scale is not None:
        if isinstance(scale, tuple):
            parts.append(f"scale = {vec(*scale)};")
        else:
            parts.append(f"scale = {f(scale)};")
    return " ".join(parts)


def instance(prefab: str, ident: str | None = None, translate: tuple[float, float, float] = (0.0, 0.0, 0.0),
             rotate: tuple[float, float, float] | None = None,
             scale: tuple[float, float, float] | float | None = None) -> str:
    ident_part = f' id = "{ident}";' if ident else ""
    return f'        {{ prefab = "{prefab}";{ident_part} transform = {{ {transform(translate, rotate, scale)} }}; }}'


def flower_prefab(name: str, petal_count: int = 10) -> str:
    petals = []
    for index in range(petal_count):
        angle = 360.0 * index / petal_count
        outer = index % 2 == 0
        tilt = -58.0 if outer else -42.0
        scale = 1.18 if outer else 0.82
        material_index = 0 if index % 2 == 0 else 1
        petals.append(mesh(material_index, "models/petal.obj",
                           transform(translate=(0.0, 0.28, 0.0),
                                     rotate=(tilt, angle, 8.0 if outer else -6.0),
                                     scale=scale)))

    return f'''    {name} = {{
        materials = {{
{metallic("glassCool", (0.08, 0.48, 0.85), 0.08)}
{metallic("glassRose", (0.92, 0.18, 0.42), 0.12)}
{emissive("core", 4.0, (0.10, 0.95, 1.0), (0.05, 0.72, 1.0))}
{metallic("vein", (0.18, 0.50, 0.56), 0.16)}
        }};
        primitives = {{
            meshes = (
{",\n".join(petals)},
{mesh(2, "models/thin_ring.obj", transform(translate=(0.0, 0.34, 0.0), rotate=(90.0, 0.0, 0.0), scale=0.55))}
            );
            spheres = (
{sphere(0, 0.14, (0.0, 0.34, 0.0))},
{sphere(2, 0.08, (0.0, 0.34, 0.0))}
            );
        }};
        lights = {{
{point_light((0.0, 1.8, 0.0), (0.55, 0.90, 1.0), 8.0, 0.06, 0.01)}
        }};
    }};'''


def spore_prefab() -> str:
    return f'''    spore = {{
        materials = {{
{emissive("glow", 2.2, (0.25, 0.80, 1.0), (0.12, 0.62, 1.0))}
        }};
        primitives = {{
            spheres = (
{sphere(0, 0.06, (0.0, 0.0, 0.0))}
            );
        }};
    }};'''


def shard_prefab() -> str:
    return f'''    shard = {{
        materials = {{
{metallic("chrome", (0.35, 0.62, 0.70), 0.12)}
{emissive("edge", 2.0, (0.18, 0.72, 1.0), (0.08, 0.52, 1.0))}
        }};
        primitives = {{
            meshes = (
{mesh(0, "models/shard.obj", transform(scale=0.7))}
            );
        }};
    }};'''


def garden_scene() -> str:
    return f'''# Garden of Impossible Glass: nested glass blooms, chrome shards and
# bioluminescent spores above a black mirror pond.
{camera((3.6, 3.2, 8.0), (0.0, 1.2, 1.5), 40.0, 0.12, 7.0)}
background:
{{
    texture = "textures/aurora_sky.hdr";
    intensity = 0.35;
}};

materials:
{{
{diffuse("pond", (0.015, 0.028, 0.045), 0.10, 0.25)}
{metallic("mirror", (0.010, 0.020, 0.035), 0.025)}
}};

animation:
{{
    fps = 24;
    shutter = {{ angle = 180.0; samples = 4; }};
    camera = (
        {{ time = 0.0; position = {vec(3.6, 3.2, 8.0)}; rotation = {vec(0.0, 1.2, 1.5)}; fieldOfView = 40.0; lensRadius = 0.12; focusDistance = 7.0; }},
        {{ time = 4.0; position = {vec(1.4, 2.3, 7.2)}; rotation = {vec(0.0, 1.3, 1.5)}; fieldOfView = 43.0; }},
        {{ time = 8.0; position = {vec(-3.0, 3.8, 7.2)}; rotation = {vec(0.0, 1.2, 1.8)}; fieldOfView = 41.0; }}
    );
    instances = (
        {{ target = "flowerCenter"; rotation = (
            {{ time = 0.0; value = {vec(0.0, 0.0, 0.0)}; }},
            {{ time = 8.0; value = {vec(4.0, 30.0, 0.0)}; }} );
          position = (
            {{ time = 0.0; value = {vec(0.0, 0.7, 1.5)}; }},
            {{ time = 4.0; value = {vec(0.0, 1.0, 1.5)}; }},
            {{ time = 8.0; value = {vec(0.0, 0.7, 1.5)}; }} );
          scale = (
            {{ time = 0.0; value = {vec(1.55, 1.55, 1.55)}; }},
            {{ time = 4.0; value = {vec(1.75, 1.75, 1.75)}; }},
            {{ time = 8.0; value = {vec(1.55, 1.55, 1.55)}; }} ); }},
        {{ target = "flowerLeft"; rotation = (
            {{ time = 0.0; value = {vec(0.0, 35.0, 0.0)}; }},
            {{ time = 8.0; value = {vec(5.0, 58.0, 0.0)}; }} ); }},
        {{ target = "flowerRight"; rotation = (
            {{ time = 0.0; value = {vec(0.0, -28.0, 0.0)}; }},
            {{ time = 8.0; value = {vec(-4.0, -8.0, 0.0)}; }} ); }}
    );
    lights = (
        {{ target = "gardenKey"; intensity = (
            {{ time = 0.0; value = 11.0; }},
            {{ time = 4.0; value = 16.0; }},
            {{ time = 8.0; value = 11.0; }} ); }},
        {{ target = "gardenWarm"; intensity = (
            {{ time = 0.0; value = 6.0; }},
            {{ time = 4.0; value = 9.0; }},
            {{ time = 8.0; value = 6.0; }} ); }}
    );
}};

prefabs:
{{
{flower_prefab("glassFlower")}
{spore_prefab()}
{shard_prefab()}
}};

primitives:
{{
    planes = (
        {{ material = 1; position = {vec(0.0, 0.0, 0.0)}; normal = {vec(0.0, 1.0, 0.0)}; }}
    );
    instances = (
        {{ prefab = "glassFlower"; id = "flowerCenter"; transform = {{ translate = {vec(0.0, 0.7, 1.5)}; scale = 1.55; }}; }},
        {{ prefab = "glassFlower"; id = "flowerLeft"; transform = {{ translate = {vec(-2.7, 0.35, 2.8)}; rotate = {vec(0.0, 35.0, 0.0)}; scale = 0.78; }}; }},
        {{ prefab = "glassFlower"; id = "flowerRight"; transform = {{ translate = {vec(2.7, 0.4, 3.6)}; rotate = {vec(0.0, -28.0, 0.0)}; scale = 0.62; }}; }}
    );
    scatter = (
        {{ prefab = "spore"; count = 60; seed = 71;
           area = {{ min = {vec(-4.5, 0.5, 2.5)}; max = {vec(4.5, 3.8, 7.5)}; }};
           scale = {{ min = 0.35; max = 0.9; }}; }},
        {{ prefab = "shard"; count = 22; seed = 23;
           area = {{ min = {vec(-4.0, 0.05, 3.0)}; max = {vec(4.0, 1.8, 7.0)}; }};
           rotation = {{ min = {vec(0.0, 0.0, 0.0)}; max = {vec(360.0, 360.0, 360.0)}; }};
           scale = {{ min = 0.25; max = 0.8; }}; }}
    );
}};

lights:
{{
    ambient = {{ color = {color(0.30, 0.48, 0.62)}; intensity = 0.10; }};
    spheres = (
{sphere_light((0.0, 5.0, 1.5), 1.6, (0.60, 0.88, 1.0), 11.0, "gardenKey")},
{sphere_light((4.2, 3.0, 4.5), 1.2, (1.0, 0.38, 0.65), 6.0, "gardenWarm")},
{sphere_light((-4.0, 2.5, 2.5), 1.0, (0.15, 0.55, 1.0), 4.0)}
    );
}};
'''


def leviathan_prefab() -> str:
    points = []
    for index in range(17):
        t = index / 16.0
        x = 2.0 * math.sin(math.pi * 1.7 * t)
        y = 0.7 + 3.8 * math.sin(math.pi * t)
        z = 1.4 + 6.4 * t
        points.append((x, y, z))

    body = []
    fins = []
    lights = []

    def tangent_at(index: int) -> tuple[float, float, float]:
        previous = points[max(index - 1, 0)]
        following = points[min(index + 1, len(points) - 1)]
        raw = tuple(following[axis] - previous[axis] for axis in range(3))
        length = math.sqrt(sum(component * component for component in raw))
        return tuple(component / length for component in raw)

    def angles_for(tangent: tuple[float, float, float]) -> tuple[float, float]:
        yaw = math.degrees(math.atan2(tangent[0], tangent[2]))
        pitch = math.degrees(-math.asin(max(-1.0, min(1.0, tangent[1]))))
        return pitch, yaw

    for index, (x, y, z) in enumerate(points):
        t = index / (len(points) - 1.0)
        tangent = tangent_at(index)
        pitch, yaw = angles_for(tangent)
        radius = 0.58 * (1.0 - 0.24 * t)
        body.append(sphere(0, radius, (0.0, 0.0, 0.0),
                           transform(translate=(x, y, z), rotate=(pitch, yaw, 0.0),
                                     scale=(1.30, 0.92, 1.85))))
        right = (tangent[2], 0.0, -tangent[0])
        right_length = math.sqrt(right[0] * right[0] + right[2] * right[2]) or 1.0
        right = (right[0] / right_length, 0.0, right[2] / right_length)
        body.append(sphere(2, 0.13, (0.0, 0.0, 0.0),
                           transform(translate=(x, y + 0.60, z), rotate=(pitch, yaw, 0.0),
                                     scale=(0.8, 0.9, 1.8))))
        angle = 35.0 + 25.0 * math.sin(t * 9.0)
        for side in (-1.0, 1.0):
            fins.append(mesh(1, "models/shard.obj",
                             transform(translate=(x + right[0] * side * (radius + 0.55),
                                                  y + 0.10,
                                                  z + right[2] * side * (radius + 0.55)),
                                       rotate=(pitch, yaw, side * angle),
                                       scale=0.95 - 0.28 * t)))
        if index == 0:
            lights.append(point_light((x, y + 0.55, z), (0.10, 0.85, 1.0), 2.0, 0.08, 0.01))

    head_tangent = tangent_at(0)
    head_pitch, head_yaw = angles_for(head_tangent)
    right = (head_tangent[2], 0.0, -head_tangent[0])
    right_length = math.sqrt(right[0] * right[0] + right[2] * right[2]) or 1.0
    right = (right[0] / right_length, 0.0, right[2] / right_length)
    head = points[0]
    body.append(sphere(0, 0.98, (0.0, 0.0, 0.0),
                       transform(translate=head, rotate=(head_pitch, head_yaw, 0.0),
                                 scale=(1.35, 1.0, 2.15))))
    body.append(sphere(3, 0.38, (0.0, 0.0, 0.0),
                       transform(translate=(head[0], head[1] - 0.48, head[2] - 0.55),
                                 rotate=(head_pitch, head_yaw, 0.0), scale=(1.5, 0.5, 1.1))))
    body.append(sphere(4, 0.10, (0.0, 0.0, 0.0),
                       transform(translate=(head[0], head[1] - 0.36, head[2] - 1.82))))
    for side in (-1.0, 1.0):
        body.append(sphere(4, 0.15, (0.0, 0.0, 0.0),
                           transform(translate=(head[0] + side * 0.48, head[1] + 0.30, head[2] - 1.75))))
        fins.append(mesh(3, "models/shard.obj",
                         transform(translate=(head[0] + head_tangent[0] * 1.35 + right[0] * side * 0.55,
                                              head[1] - 0.25,
                                              head[2] + head_tangent[2] * 1.35 + right[2] * side * 0.55),
                                   rotate=(head_pitch, head_yaw, side * 18.0), scale=0.5)))

    return f'''    leviathan = {{
        materials = {{
{metallic("chrome", (0.025, 0.070, 0.105), 0.06)}
{dielectric("finGlass", (0.18, 0.62, 0.85), 1.40)}
{emissive("spine", 5.5, (0.10, 0.90, 1.0), (0.04, 0.68, 1.0))}
{metallic("jaw", (0.015, 0.035, 0.055), 0.22)}
{emissive("eye", 8.0, (1.0, 0.32, 0.12), (1.0, 0.18, 0.04))}
        }};
        primitives = {{
            spheres = (
{",".join(body)}
            );
            meshes = (
{",".join(fins)}
            );
        }};
        lights = {{
{chr(10).join(lights)}
        }};
    }}'''


def leviathan_scene() -> str:
    return f'''# The Last Leviathan: a chrome-and-glass serpent breaching a black mirror
# sea beneath an eclipse.
{camera((0.0, 1.2, -9.0), (0.0, 3.2, 4.0), 48.0, 0.05, 10.0)}
background:
{{
    texture = "textures/eclipse_sky.hdr";
    intensity = 0.85;
}};

materials:
{{
{metallic("sea", (0.008, 0.018, 0.030), 0.018)}
{diffuse("rock", (0.025, 0.035, 0.045), 0.12, 0.35)}
{emissive("eclipseRing", 4.0, (0.10, 0.42, 1.0), (0.04, 0.30, 1.0))}
}};

prefabs:
{{
{leviathan_prefab()}
{shard_prefab()}
}};

primitives:
{{
    planes = (
        {{ material = 0; position = {vec(0.0, 0.0, 0.0)}; normal = {vec(0.0, 1.0, 0.0)}; }}
    );
    meshes = (
        {{ material = 2; path = "models/thin_ring.obj"; transform = {{ translate = {vec(0.0, 4.8, -5.0)}; rotate = {vec(0.0, 0.0, 0.0)}; scale = 6.0; }}; }}
    );
    instances = (
{instance("leviathan", "leviathan", (0.0, 0.0, 0.0), (0.0, 0.0, 0.0), 1.0)}
    );
    scatter = (
        {{ prefab = "shard"; count = 46; seed = 17;
           area = {{ min = {vec(-6.0, 0.05, 2.0)}; max = {vec(6.0, 2.0, 9.0)}; }};
           rotation = {{ min = {vec(0.0, 0.0, 0.0)}; max = {vec(360.0, 360.0, 360.0)}; }};
           scale = {{ min = 0.2; max = 0.7; }}; }}
    );
}};

lights:
{{
    ambient = {{ color = {color(0.22, 0.38, 0.55)}; intensity = 0.06; }};
    spheres = (
{sphere_light((0.0, 5.0, -3.0), 3.0, (0.30, 0.65, 1.0), 8.0)},
{sphere_light((-6.0, 2.5, 2.0), 1.5, (1.0, 0.28, 0.12), 5.0)},
{sphere_light((6.0, 4.0, 6.0), 1.6, (0.20, 0.60, 1.0), 4.0)}
    );
}};
'''


def cathedral_scene() -> str:
    pillar = f'''    pillar = {{
        materials = {{
{diffuse("marble", (0.55, 0.58, 0.62), 0.22, 0.75, "textures/dark_marble.ppm")}
{metallic("gold", (0.85, 0.48, 0.14), 0.16)}
{emissive("lamp", 3.0, (1.0, 0.62, 0.22), (1.0, 0.38, 0.08))}
        }};
        primitives = {{
            meshes = (
{mesh(0, "models/monolith.obj", transform(translate=(0.0, 0.0, 0.0), scale=(1.2, 1.65, 1.2)))},
{mesh(1, "models/thin_ring.obj", transform(translate=(0.0, 5.5, 0.0), rotate=(90.0, 0.0, 0.0), scale=1.7))},
{mesh(1, "models/thin_ring.obj", transform(translate=(0.0, 2.5, 0.0), rotate=(90.0, 0.0, 0.0), scale=1.2))}
            );
            spheres = (
{sphere(2, 0.20, (0.0, 5.5, 0.0))}
            );
        }};
        lights = {{
{point_light((0.0, 5.5, 0.0), (1.0, 0.52, 0.18), 1.2, 0.08, 0.015)}
        }};
    }};'''

    machine_sun = f'''    machineSun = {{
        materials = {{
{dielectric("sunGlass", (0.85, 0.78, 0.55), 1.46)}
{metallic("sunMetal", (0.90, 0.48, 0.12), 0.20)}
{emissive("sunCore", 9.0, (1.0, 0.68, 0.22), (1.0, 0.34, 0.04))}
{emissive("sunEdge", 2.5, (0.20, 0.72, 1.0), (0.04, 0.42, 1.0))}
        }};
        primitives = {{
            meshes = (
{mesh(1, "models/thin_ring.obj", transform(translate=(0.0, 0.0, 0.0), rotate=(90.0, 0.0, 0.0), scale=2.6))},
{mesh(1, "models/thin_ring.obj", transform(translate=(0.0, 0.0, 0.0), rotate=(62.0, 18.0, 0.0), scale=3.2))},
{mesh(3, "models/thin_ring.obj", transform(translate=(0.0, 0.0, 0.0), rotate=(24.0, 78.0, 0.0), scale=1.8))}
            );
            spheres = (
{sphere(0, 1.85, (0.0, 0.0, 0.0))},
{sphere(2, 1.05, (0.0, 0.0, 0.0))}
            );
        }};
        lights = {{
{point_light((0.0, 0.0, 0.0), (1.0, 0.58, 0.20), 6.0, 0.02, 0.001)}
        }};
    }};'''

    pillar_instances = []
    for side in (-1.0, 1.0):
        for index, z in enumerate((0.0, 3.0, 6.0, 9.0, 12.0)):
            pillar_instances.append(instance("pillar", f"pillar{side:+.0f}_{index}",
                                             (side * 4.8, 0.0, z), (0.0, 0.0, 0.0), 1.0))

    return f'''# Cathedral of the Machine Sun: a brass-and-glass singularity suspended
# inside a nave of dark marble pillars.
{camera((0.0, 3.2, -12.0), (0.0, 5.2, 5.5), 55.0, 0.05, 15.0)}
background:
{{
    texture = "textures/eclipse_sky.hdr";
    intensity = 0.40;
}};

materials:
{{
{metallic("floor", (0.010, 0.018, 0.028), 0.035)}
{diffuse("altar", (0.06, 0.055, 0.050), 0.18, 0.70)}
{metallic("gold", (0.80, 0.44, 0.12), 0.16)}
{dielectric("orbGlass", (0.55, 0.82, 1.0), 1.45)}
}};

prefabs:
{{
{pillar}
{machine_sun}
}};

primitives:
{{
    planes = (
        {{ material = 0; position = {vec(0.0, 0.0, 0.0)}; normal = {vec(0.0, 1.0, 0.0)}; }}
    );
    boxes = (
        {{ material = 1; min = {vec(-2.0, 0.0, 5.0)}; max = {vec(2.0, 0.6, 9.0)}; }}
    );
    meshes = (
        {{ material = 2; path = "models/thin_ring.obj"; transform = {{ translate = {vec(0.0, 6.2, 11.0)}; scale = 5.2; }}; }}
    );
    spheres = (
        {{ material = 3; r = 0.32; position = {vec(-2.2, 4.2, 4.0)}; }},
        {{ material = 3; r = 0.25; position = {vec(2.5, 3.6, 5.0)}; }},
        {{ material = 3; r = 0.38; position = {vec(-3.0, 3.0, 7.0)}; }},
        {{ material = 3; r = 0.22; position = {vec(3.2, 4.5, 7.5)}; }}
    );
    instances = (
{",".join(pillar_instances)},
{instance("machineSun", "machineSun", (0.0, 6.2, 7.0), (0.0, 0.0, 0.0), 1.0)}
    );
}};

lights:
{{
    ambient = {{ color = {color(0.42, 0.38, 0.30)}; intensity = 0.08; }};
    spheres = (
{sphere_light((0.0, 10.0, 6.0), 3.2, (1.0, 0.55, 0.16), 24.0)},
{sphere_light((-6.5, 4.0, 5.0), 1.5, (0.20, 0.55, 1.0), 8.0)},
{sphere_light((6.5, 4.0, 5.0), 1.5, (1.0, 0.20, 0.48), 8.0)}
    );
}};
'''


def serpent_scene() -> str:
    points = []
    for index in range(25):
        t = index / 24.0
        x = -3.8 + 7.6 * t
        y = 2.3 + 2.5 * math.sin(2.0 * math.pi * t + 0.25)
        z = 1.4 + 1.1 * math.sin(3.0 * math.pi * t)
        points.append((x, y, z))

    def tangent_at(index: int) -> tuple[float, float, float]:
        previous = points[max(index - 1, 0)]
        following = points[min(index + 1, len(points) - 1)]
        raw = tuple(following[axis] - previous[axis] for axis in range(3))
        length = math.sqrt(sum(component * component for component in raw))
        return tuple(component / length for component in raw)

    shards = []
    glow = []
    for index, (x, y, z) in enumerate(points):
        t = index / (len(points) - 1.0)
        tangent = tangent_at(index)
        yaw = math.degrees(math.atan2(tangent[0], tangent[2]))
        pitch = math.degrees(-math.asin(max(-1.0, min(1.0, tangent[1]))))
        size = 0.52 + 0.52 * math.sin(math.pi * t)
        material_index = 1 if index % 4 == 0 else 0
        shards.append(mesh(material_index, "models/shard.obj",
                           transform(translate=(x, y, z), rotate=(pitch, yaw, 8.0 * math.sin(t * 9.0)),
                                     scale=size)))
        if index % 2 == 0:
            glow.append(sphere(2, 0.07, (0.0, 0.0, 0.0),
                               transform(translate=(x, y + 0.22, z), scale=(0.7, 0.7, 1.4))))

    head = points[0]
    head_tangent = tangent_at(0)
    head_yaw = math.degrees(math.atan2(head_tangent[0], head_tangent[2]))
    glow.extend([
        sphere(2, 0.20, (0.0, 0.0, 0.0), transform(translate=(head[0] - 0.35, head[1] + 0.18, head[2] - 0.2))),
        sphere(2, 0.20, (0.0, 0.0, 0.0), transform(translate=(head[0] + 0.35, head[1] + 0.18, head[2] - 0.2))),
        sphere(0, 0.62, (0.0, 0.0, 0.0),
               transform(translate=head, rotate=(0.0, head_yaw, 0.0), scale=(1.2, 0.9, 1.5))),
    ])

    star_serpent = f'''    starSerpent = {{
        materials = {{
{metallic("shardChrome", (0.10, 0.34, 0.48), 0.08)}
{emissive("shardGlow", 5.5, (0.10, 0.85, 1.0), (0.03, 0.55, 1.0))}
{emissive("spineGlow", 7.0, (1.0, 0.38, 0.58), (1.0, 0.12, 0.32))}
        }};
        primitives = {{
            meshes = (
{",".join(shards)}
            );
            spheres = (
{",".join(glow)}
            );
        }};
    }};'''

    return f'''# Serpent of Falling Stars: a crystal serpent coils through a field of
# falling shards above a black mirror sea.
{camera((0.0, 2.0, -11.0), (0.0, 3.6, 2.0), 52.0, 0.06, 11.0)}
background:
{{
    texture = "textures/aurora_sky.hdr";
    intensity = 0.65;
}};

animation:
{{
    fps = 24;
    shutter = {{ angle = 180.0; samples = 5; }};
    camera = (
        {{ time = 0.0; position = {vec(0.0, 2.0, -11.0)}; rotation = {vec(0.0, 3.6, 2.0)}; fieldOfView = 52.0; lensRadius = 0.06; focusDistance = 11.0; }},
        {{ time = 4.0; position = {vec(6.5, 4.0, -7.0)}; rotation = {vec(0.0, 3.8, 2.0)}; fieldOfView = 56.0; }},
        {{ time = 8.0; position = {vec(0.0, 6.8, -9.0)}; rotation = {vec(0.0, 4.0, 2.0)}; fieldOfView = 54.0; }}
    );
    instances = (
        {{ target = "serpent"; rotation = (
            {{ time = 0.0; value = {vec(0.0, 0.0, 0.0)}; }},
            {{ time = 8.0; value = {vec(0.0, 24.0, 6.0)}; }} );
          position = (
            {{ time = 0.0; value = {vec(0.0, 0.0, 0.0)}; }},
            {{ time = 4.0; value = {vec(0.0, 0.45, 0.0)}; }},
            {{ time = 8.0; value = {vec(0.0, 0.0, 0.0)}; }} );
          scale = (
            {{ time = 0.0; value = {vec(1.0, 1.0, 1.0)}; }},
            {{ time = 4.0; value = {vec(1.08, 1.08, 1.08)}; }},
            {{ time = 8.0; value = {vec(1.0, 1.0, 1.0)}; }} ); }}
    );
    lights = (
        {{ target = "serpentKey"; intensity = (
            {{ time = 0.0; value = 16.0; }},
            {{ time = 4.0; value = 24.0; }},
            {{ time = 8.0; value = 16.0; }} ); }},
        {{ target = "serpentCore"; intensity = (
            {{ time = 0.0; value = 7.0; }},
            {{ time = 4.0; value = 12.0; }},
            {{ time = 8.0; value = 7.0; }} ); }}
    );
}};

materials:
{{
{metallic("sea", (0.008, 0.018, 0.030), 0.018)}
{metallic("monolithMetal", (0.020, 0.035, 0.050), 0.18)}
{metallic("gold", (0.78, 0.42, 0.10), 0.14)}
{dielectric("orb", (0.45, 0.75, 1.0), 1.42)}
}};

prefabs:
{{
{star_serpent}
}};

primitives:
{{
    planes = (
        {{ material = 0; position = {vec(0.0, 0.0, 0.0)}; normal = {vec(0.0, 1.0, 0.0)}; }}
    );
    meshes = (
        {{ material = 1; path = "models/monolith.obj"; transform = {{ translate = {vec(0.0, 0.0, 6.0)}; scale = {vec(1.1, 1.55, 1.1)}; }}; }},
        {{ material = 2; path = "models/thin_ring.obj"; transform = {{ translate = {vec(0.0, 5.8, 6.0)}; rotate = {vec(90.0, 0.0, 0.0)}; scale = 2.2; }}; }}
    );
    spheres = (
        {{ material = 3; r = 0.55; position = {vec(0.0, 5.8, 6.0)}; }}
    );
    instances = (
{instance("starSerpent", "serpent", (0.0, 0.0, 0.0), (0.0, 0.0, 0.0), 1.0)}
    );
}};

lights:
{{
    ambient = {{ color = {color(0.25, 0.38, 0.55)}; intensity = 0.07; }};
    spheres = (
{sphere_light((0.0, 7.0, -3.0), 2.5, (0.25, 0.62, 1.0), 16.0, "serpentKey")},
{sphere_light((-5.0, 4.0, 4.0), 1.4, (1.0, 0.30, 0.52), 8.0)}
    );
    point = (
        {{ id = "serpentCore"; position = {vec(0.0, 5.8, 6.0)}; color = {color(1.0, 0.55, 0.18)}; intensity = 7.0; attenuation = {{ constant = 1.0; linear = 0.05; quadratic = 0.005; }}; }}
    );
}};
'''


def clockwork_scene() -> str:
    ring = f'''    orbitalRing = {{
        materials = {{
{metallic("brass", (0.76, 0.42, 0.12), 0.18)}
{metallic("chrome", (0.30, 0.60, 0.75), 0.07)}
{dielectric("lens", (0.45, 0.78, 1.0), 1.45)}
{emissive("spark", 4.0, (0.18, 0.80, 1.0), (0.04, 0.48, 1.0))}
        }};
        primitives = {{
            meshes = (
{mesh(0, "models/thin_ring.obj", transform(translate=(0.0, 0.0, 0.0), scale=3.7))},
{mesh(1, "models/torus.obj", transform(translate=(0.0, 0.0, 0.0), scale=2.2))}
            );
            spheres = (
{",".join(sphere(2, 0.26, (math.cos(2.0 * math.pi * i / 6.0) * 3.7,
                           math.sin(2.0 * math.pi * i / 6.0) * 3.7, 0.0)) for i in range(6))},
{",".join(sphere(3, 0.10, (math.cos(2.0 * math.pi * i / 6.0) * 3.7,
                           math.sin(2.0 * math.pi * i / 6.0) * 3.7, 0.12)) for i in range(6))}
            );
        }};
        lights = {{
{point_light((3.7, 0.0, 0.0), (0.35, 0.82, 1.0), 1.8, 0.10, 0.02)}
        }};
    }};'''

    return f'''# Clockwork Aurora: a brass-and-glass orbital engine turning in a
# starfield above a black mirror.
{camera((0.0, 2.0, -14.0), (0.0, 5.0, 3.0), 55.0, 0.05, 15.0)}
background:
{{
    texture = "textures/aurora_sky.hdr";
    intensity = 0.72;
}};

animation:
{{
    fps = 24;
    shutter = {{ angle = 180.0; samples = 5; }};
    camera = (
        {{ time = 0.0; position = {vec(0.0, 2.0, -14.0)}; rotation = {vec(0.0, 5.0, 3.0)}; fieldOfView = 55.0; lensRadius = 0.05; focusDistance = 15.0; }},
        {{ time = 4.0; position = {vec(7.0, 4.0, -7.0)}; rotation = {vec(0.0, 5.2, 3.0)}; fieldOfView = 58.0; }},
        {{ time = 8.0; position = {vec(0.0, 8.0, -11.0)}; rotation = {vec(0.0, 5.5, 3.0)}; fieldOfView = 54.0; }}
    );
    instances = (
        {{ target = "ringA"; rotation = (
            {{ time = 0.0; value = {vec(90.0, 0.0, 0.0)}; }},
            {{ time = 8.0; value = {vec(90.0, 180.0, 0.0)}; }} ); }},
        {{ target = "ringB"; rotation = (
            {{ time = 0.0; value = {vec(62.0, 28.0, 8.0)}; }},
            {{ time = 8.0; value = {vec(62.0, 208.0, 8.0)}; }} ); }},
        {{ target = "ringC"; rotation = (
            {{ time = 0.0; value = {vec(24.0, 68.0, -12.0)}; }},
            {{ time = 8.0; value = {vec(24.0, 248.0, -12.0)}; }} ); }}
    );
    lights = (
        {{ target = "clockKey"; intensity = (
            {{ time = 0.0; value = 18.0; }},
            {{ time = 4.0; value = 26.0; }},
            {{ time = 8.0; value = 18.0; }} ); }},
        {{ target = "clockCore"; intensity = (
            {{ time = 0.0; value = 9.0; }},
            {{ time = 4.0; value = 14.0; }},
            {{ time = 8.0; value = 9.0; }} ); }}
    );
}};

materials:
{{
{metallic("sea", (0.008, 0.018, 0.030), 0.018)}
{dielectric("coreGlass", (0.55, 0.80, 1.0), 1.48)}
{emissive("coreGlow", 7.0, (0.20, 0.82, 1.0), (0.04, 0.52, 1.0))}
}};

prefabs:
{{
{ring}
}};

primitives:
{{
    planes = (
        {{ material = 0; position = {vec(0.0, 0.0, 0.0)}; normal = {vec(0.0, 1.0, 0.0)}; }}
    );
    spheres = (
        {{ material = 1; r = 2.0; position = {vec(0.0, 5.0, 3.0)}; }},
        {{ material = 2; r = 0.95; position = {vec(0.0, 5.0, 3.0)}; }}
    );
    instances = (
{instance("orbitalRing", "ringA", (0.0, 5.0, 3.0), (90.0, 0.0, 0.0), 1.0)},
{instance("orbitalRing", "ringB", (0.0, 5.0, 3.0), (62.0, 28.0, 8.0), 0.82)},
{instance("orbitalRing", "ringC", (0.0, 5.0, 3.0), (24.0, 68.0, -12.0), 0.65)}
    );
}};

lights:
{{
    ambient = {{ color = {color(0.30, 0.45, 0.62)}; intensity = 0.08; }};
    spheres = (
{sphere_light((0.0, 9.0, 3.0), 2.4, (0.30, 0.72, 1.0), 18.0, "clockKey")},
{sphere_light((-6.0, 5.0, 6.0), 1.4, (1.0, 0.36, 0.55), 7.0)}
    );
    point = (
        {{ id = "clockCore"; position = {vec(0.0, 5.0, 3.0)}; color = {color(0.10, 0.72, 1.0)}; intensity = 9.0; attenuation = {{ constant = 1.0; linear = 0.04; quadratic = 0.002; }}; }}
    );
}};
'''


def main() -> None:
    SCENES.mkdir(parents=True, exist_ok=True)
    (SCENES / "impossible_glass_garden.cfg").write_text(garden_scene())
    (SCENES / "last_leviathan.cfg").write_text(leviathan_scene())
    (SCENES / "cathedral_machine_sun.cfg").write_text(cathedral_scene())
    (SCENES / "serpent_of_stars.cfg").write_text(serpent_scene())
    (SCENES / "clockwork_aurora.cfg").write_text(clockwork_scene())
    print("wrote impossible_glass_garden.cfg")
    print("wrote last_leviathan.cfg")
    print("wrote cathedral_machine_sun.cfg")
    print("wrote serpent_of_stars.cfg")
    print("wrote clockwork_aurora.cfg")


if __name__ == "__main__":
    main()
