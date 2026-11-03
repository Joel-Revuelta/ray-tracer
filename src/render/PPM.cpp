/*
** EPITECH PROJECT, 2024
** raytracer
** File description:
** PPM
*/

#include "plugins/renderers/PPM.hpp"
#include <cstddef>
#include <fstream>
#include <stdexcept>

using namespace RayTracer;

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
    for (std::size_t i = 0; i < pixelCount; ++i)
    {
        const uint32_t pixel = pixels[i];
        const unsigned char rgb[3] = {static_cast<unsigned char>(pixel & 0xFFu),
                                      static_cast<unsigned char>((pixel >> 8u) & 0xFFu),
                                      static_cast<unsigned char>((pixel >> 16u) & 0xFFu)};

        out.write(reinterpret_cast<const char *>(rgb), 3);
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
