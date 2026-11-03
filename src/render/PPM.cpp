/*
** EPITECH PROJECT, 2024
** raytracer
** File description:
** PPM
*/

#include "plugins/renderers/PPM.hpp"
#include "Color.hpp"
#include "Utils.hpp"
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

    out << "P3" << std::endl << width << " " << height << "\n255" << std::endl;
    for (std::size_t i = 0; i < pixelCount; ++i)
        out << Utils::convertTo255(Utils::convertFromRGBA(pixels[i])) << std::endl;

    if (!out.good())
        throw std::runtime_error("Failed to write the rendered image");
}

void RayTracer::writePPMFile(const std::string &path, int width, int height, const std::vector<uint32_t> &pixels)
{
    std::ofstream file(path, std::ios::out);

    if (!file.is_open())
        throw std::runtime_error("Cannot open output file: " + path);

    writePPM(file, width, height, pixels);
}
