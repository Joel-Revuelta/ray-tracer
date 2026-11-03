#!/usr/bin/env python3
"""Generate the equirectangular sky texture used by the environment scene."""

from __future__ import annotations

import math
from pathlib import Path


def clamp(value: float, minimum: float = 0.0, maximum: float = 1.0) -> float:
    return max(minimum, min(maximum, value))


def main() -> None:
    width, height = 512, 256
    sun_azimuth = math.pi / 2.0
    sun_elevation = math.radians(20.0)
    sun_direction = (
        math.sin(math.pi / 2.0 - sun_elevation) * math.cos(sun_azimuth),
        math.cos(math.pi / 2.0 - sun_elevation),
        math.sin(math.pi / 2.0 - sun_elevation) * math.sin(sun_azimuth),
    )
    sun_cosine = math.cos(math.radians(4.5))

    data = bytearray()

    for y in range(height):
        v = (y + 0.5) / height
        phi = v * math.pi
        sin_phi = math.sin(phi)
        cos_phi = math.cos(phi)

        for x in range(width):
            u = (x + 0.5) / width
            theta = (u - 0.5) * 2.0 * math.pi
            direction = (
                sin_phi * math.cos(theta),
                cos_phi,
                sin_phi * math.sin(theta),
            )

            if direction[1] >= 0.0:
                blend = math.sqrt(direction[1])
                zenith = (0.01, 0.03, 0.12)
                horizon = (0.22, 0.30, 0.42)
                color = tuple(horizon[i] * (1.0 - blend) + zenith[i] * blend for i in range(3))
            else:
                blend = math.sqrt(-direction[1])
                horizon = (0.10, 0.09, 0.08)
                ground = (0.02, 0.02, 0.02)
                color = tuple(horizon[i] * (1.0 - blend) + ground[i] * blend for i in range(3))

            cosine = (
                direction[0] * sun_direction[0]
                + direction[1] * sun_direction[1]
                + direction[2] * sun_direction[2]
            )
            glow = pow(max(cosine, 0.0), 180.0) * 0.7

            if cosine > sun_cosine:
                sun = (1.0, 0.93, 0.78)
                color = tuple(min(1.0, color[i] + sun[i]) for i in range(3))
            else:
                color = tuple(min(1.0, color[i] + glow) for i in range(3))

            data.extend(int(clamp(channel) * 255.0 + 0.5) for channel in color)

    output = Path(__file__).resolve().parent.parent / "scenes" / "textures" / "sky.ppm"
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_bytes(f"P6\n{width} {height}\n255\n".encode() + bytes(data))


if __name__ == "__main__":
    main()
