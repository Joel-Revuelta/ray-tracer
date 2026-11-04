#!/usr/bin/env python3
"""Generate deterministic procedural textures and the monolith mesh.

All textures are written as binary P6 PPM files in sRGB space, matching the
renderer's texture loader. The generator only uses the Python standard library
so it can run in the same environments as the other test asset scripts.
"""

from __future__ import annotations

import math
import random
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
TEXTURES = ROOT / "scenes" / "textures"
MODELS = ROOT / "scenes" / "models"


def write_ppm(path: Path, width: int, height: int, pixels: bytearray) -> None:
    assert len(pixels) == width * height * 3
    with path.open("wb") as handle:
        handle.write(f"P6\n{width} {height}\n255\n".encode("ascii"))
        handle.write(pixels)


def clamp_byte(value: float) -> int:
    return max(0, min(255, int(round(value))))


def generate_stone_tiles() -> None:
    width = height = 256
    tile = 32
    rng = random.Random(1103)
    pixels = bytearray()

    for y in range(height):
        for x in range(width):
            tx = x // tile
            ty = y // tile
            grout = (x % tile < 3) or (y % tile < 3)
            variation = ((tx * 17 + ty * 29) % 19) - 9
            noise = rng.randint(-5, 5)

            if grout:
                base = 28 + variation // 2 + noise
                color = (base, base + 2, base + 3)
            else:
                base = 78 + variation + noise
                color = (base, base + 4, base + 7)

                # Sparse moss creeping out of the tile seams.
                if rng.random() < 0.018:
                    color = (color[0] - 18, color[1] + 24, color[2] - 4)

            pixels.extend(clamp_byte(channel) for channel in color)

    write_ppm(TEXTURES / "stone_tiles.ppm", width, height, pixels)


def generate_dark_marble() -> None:
    width = height = 256
    rng = random.Random(2207)
    pixels = bytearray()

    for y in range(height):
        for x in range(width):
            u = x / width
            v = y / height
            warp = math.sin(v * 11.0 + math.sin(u * 7.0) * 2.1)
            vein_field = math.sin(u * 15.0 + warp * 2.4) + 0.55 * math.sin(v * 31.0 - u * 9.0)
            vein = max(0.0, 1.0 - abs(vein_field) * 3.4)
            grain = rng.randint(-4, 4)
            base = 13 + grain

            color = (
                base + 42.0 * vein,
                base + 31.0 * vein,
                base + 15.0 * vein,
            )
            pixels.extend(clamp_byte(channel) for channel in color)

    write_ppm(TEXTURES / "dark_marble.ppm", width, height, pixels)


def generate_ice_cracks() -> None:
    width = height = 256
    rng = random.Random(3301)
    pixels = bytearray()

    for y in range(height):
        for x in range(width):
            u = x / width
            v = y / height
            wash = 8.0 * math.sin(u * 9.0 + v * 5.0) + 5.0 * math.sin(v * 21.0)
            base = 174 + wash + rng.randint(-4, 4)
            color = (base - 22, base - 4, base + 12)
            pixels.extend(clamp_byte(channel) for channel in color)

    def set_pixel(x: int, y: int, color: tuple[int, int, int]) -> None:
        if 0 <= x < width and 0 <= y < height:
            offset = (y * width + x) * 3
            pixels[offset:offset + 3] = bytes(color)

    def draw_line(x0: int, y0: int, x1: int, y1: int, color: tuple[int, int, int]) -> None:
        dx = abs(x1 - x0)
        sx = 1 if x0 < x1 else -1
        dy = -abs(y1 - y0)
        sy = 1 if y0 < y1 else -1
        error = dx + dy

        while True:
            set_pixel(x0, y0, color)
            if x0 == x1 and y0 == y1:
                break
            doubled = 2 * error
            if doubled >= dy:
                error += dy
                x0 += sx
            if doubled <= dx:
                error += dx
                y0 += sy

    for _ in range(26):
        x = rng.randrange(width)
        y = rng.randrange(height)
        angle = rng.uniform(0.0, 2.0 * math.pi)
        length = rng.randint(18, 90)
        x1 = x + int(math.cos(angle) * length)
        y1 = y + int(math.sin(angle) * length)
        draw_line(x, y, x1, y1, (232, 246, 255))

        if rng.random() < 0.55:
            draw_line(x, y + 1, x1, y1 + 1, (102, 143, 176))

    write_ppm(TEXTURES / "ice_cracks.ppm", width, height, pixels)


def generate_nebula_equirect() -> None:
    width = 1024
    height = 512
    rng = random.Random(4409)
    pixels = bytearray()

    for y in range(height):
        v = y / (height - 1)

        for x in range(width):
            u = x / (width - 1)
            horizon = math.exp(-((v - 0.58) ** 2) / (2.0 * 0.16 ** 2))
            upper = math.exp(-((v - 0.28) ** 2) / (2.0 * 0.22 ** 2))
            cloud = (
                math.sin(u * 13.0 + math.sin(v * 9.0) * 2.0)
                + 0.6 * math.sin(u * 27.0 - v * 17.0)
                + 0.35 * math.sin(u * 51.0 + v * 33.0)
            )
            cloud = max(0.0, cloud * 0.5 + 0.5)

            teal = cloud * horizon
            violet = cloud * upper * 0.7
            base = 9.0 + 8.0 * (1.0 - v)

            color = (
                base + 42.0 * violet + 14.0 * horizon,
                base + 78.0 * teal + 18.0 * horizon,
                base + 92.0 * teal + 58.0 * violet + 24.0 * horizon,
            )
            pixels.extend(clamp_byte(channel) for channel in color)

    # Stars are sparse and deliberately sub-pixel-bright.
    for _ in range(900):
        x = rng.randrange(width)
        y = rng.randrange(height)
        brightness = rng.randint(90, 255)
        offset = (y * width + x) * 3
        pixels[offset:offset + 3] = bytes(
            (brightness, min(255, brightness + 8), min(255, brightness + 20))
        )

    write_ppm(TEXTURES / "nebula_equirect.ppm", width, height, pixels)


def generate_thin_ring() -> None:
    major_segments = 64
    minor_segments = 10
    major_radius = 1.0
    minor_radius = 0.035
    vertices: list[tuple[float, float, float]] = []

    for major in range(major_segments):
        theta = 2.0 * math.pi * major / major_segments
        for minor in range(minor_segments):
            phi = 2.0 * math.pi * minor / minor_segments
            radius = major_radius + minor_radius * math.cos(phi)
            vertices.append(
                (
                    radius * math.cos(theta),
                    minor_radius * math.sin(phi),
                    radius * math.sin(theta),
                )
            )

    faces: list[tuple[int, int, int, int]] = []

    for major in range(major_segments):
        next_major = (major + 1) % major_segments
        for minor in range(minor_segments):
            next_minor = (minor + 1) % minor_segments
            a = major * minor_segments + minor
            b = next_major * minor_segments + minor
            c = next_major * minor_segments + next_minor
            d = major * minor_segments + next_minor
            faces.append((a + 1, b + 1, c + 1, d + 1))

    lines = ["# Generated by tests/generate_scene_assets.py", "o thin_ring"]
    lines.extend(f"v {x:.6f} {y:.6f} {z:.6f}" for x, y, z in vertices)
    lines.extend("f " + " ".join(str(index) for index in face) for face in faces)

    (MODELS / "thin_ring.obj").write_text("\n".join(lines) + "\n", encoding="utf-8")


def generate_monolith() -> None:
    vertices = [
        (-0.60, 0.0, -0.60),
        (0.60, 0.0, -0.60),
        (0.60, 0.0, 0.60),
        (-0.60, 0.0, 0.60),
        (-0.25, 5.0, -0.25),
        (0.25, 5.0, -0.25),
        (0.25, 5.0, 0.25),
        (-0.25, 5.0, 0.25),
    ]
    faces = [
        (1, 2, 3, 4),
        (5, 6, 7, 8),
        (1, 2, 6, 5),
        (2, 3, 7, 6),
        (3, 4, 8, 7),
        (4, 1, 5, 8),
    ]

    lines = ["# Generated by tests/generate_scene_assets.py", "o monolith"]
    lines.extend(f"v {x:.6f} {y:.6f} {z:.6f}" for x, y, z in vertices)
    lines.extend("f " + " ".join(str(index) for index in face) for face in faces)

    (MODELS / "monolith.obj").write_text("\n".join(lines) + "\n", encoding="utf-8")


def main() -> None:
    TEXTURES.mkdir(parents=True, exist_ok=True)
    MODELS.mkdir(parents=True, exist_ok=True)

    generate_stone_tiles()
    generate_dark_marble()
    generate_ice_cracks()
    generate_nebula_equirect()
    generate_thin_ring()
    generate_monolith()


if __name__ == "__main__":
    main()
