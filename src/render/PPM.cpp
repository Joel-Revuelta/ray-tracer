/*
** EPITECH PROJECT, 2024
** raytracer
** File description:
** PPM
*/

#include "plugins/renderers/PPM.hpp"
#include "render/Texture.hpp"
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <fstream>
#include <memory>
#include <stdexcept>

using namespace RayTracer;

namespace
{
std::uint32_t toByte(float value)
{
    const float clamped = std::clamp(value, 0.0f, 1.0f);

    return static_cast<std::uint32_t>(std::lround(clamped * 255.0f));
}
} // namespace

void RayTracer::writePPM(std::ostream &out, int width, int height, const std::vector<uint32_t> &pixels)
{
    if (width <= 0 || height <= 0)
        throw std::invalid_argument("PPM dimensions must be positive");

    const std::size_t pixelCount = static_cast<std::size_t>(width) * static_cast<std::size_t>(height);

    if (pixels.size() < pixelCount)
        throw std::invalid_argument("Not enough pixel data for the requested PPM size");

    // Binary P6 output. The renderer stores pixels as RGBA with red in the low
    // byte, so write the three colour bytes directly.
    out << "P6\n" << width << " " << height << "\n255\n";

    std::vector<unsigned char> row(static_cast<std::size_t>(width) * 3u);

    for (int y = 0; y < height; ++y)
    {
        for (int x = 0; x < width; ++x)
        {
            const uint32_t pixel = pixels[static_cast<std::size_t>(y) * static_cast<std::size_t>(width) +
                                          static_cast<std::size_t>(x)];
            const std::size_t offset = static_cast<std::size_t>(x) * 3u;

            row[offset] = static_cast<unsigned char>(pixel & 0xFFu);
            row[offset + 1u] = static_cast<unsigned char>((pixel >> 8u) & 0xFFu);
            row[offset + 2u] = static_cast<unsigned char>((pixel >> 16u) & 0xFFu);
        }

        out.write(reinterpret_cast<const char *>(row.data()), static_cast<std::streamsize>(row.size()));
    }

    if (!out.good())
        throw std::runtime_error("Failed to write the rendered image");
}

void RayTracer::writePPMFile(const std::string &path, int width, int height, const std::vector<uint32_t> &pixels)
{
    std::ofstream file(path, std::ios::out | std::ios::binary);

    if (!file.is_open())
        throw std::runtime_error("Cannot open output file: " + path);

    writePPM(file, width, height, pixels);
}

PPMImage RayTracer::readPPMFile(const std::string &path)
{
    // The texture loader already implements the strict P3/P6 header and
    // component validation. Linear decoding preserves the stored 8-bit
    // values, which are then packed into the renderer's display layout.
    const std::shared_ptr<const Texture> texture = loadPPMTexture(path, TextureColorSpace::LINEAR);

    if (!texture)
        throw std::runtime_error("Cannot read PPM image: " + path);

    const int width = texture->getWidth();
    const int height = texture->getHeight();

    if (width <= 0 || height <= 0)
        throw std::runtime_error("PPM image has invalid dimensions: " + path);

    const std::vector<Color3D> &source = texture->getPixels();

    PPMImage image;
    image.width = static_cast<std::size_t>(width);
    image.height = static_cast<std::size_t>(height);
    image.pixels.reserve(source.size());

    for (const Color3D &color : source)
    {
        const std::uint32_t red = toByte(color.getX());
        const std::uint32_t green = toByte(color.getY());
        const std::uint32_t blue = toByte(color.getZ());

        image.pixels.push_back((0xFFu << 24u) | (blue << 16u) | (green << 8u) | red);
    }

    return image;
}
