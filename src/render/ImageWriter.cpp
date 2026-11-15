/*
** EPITECH PROJECT, 2024
** raytracer
** File description:
** ImageWriter
*/

#include "render/ImageWriter.hpp"
#include "plugins/renderers/PPM.hpp"
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"
#include <algorithm>
#include <cctype>
#include <cstdio>
#include <filesystem>
#include <stdexcept>

using namespace RayTracer;

namespace
{
std::string lowerExtension(const std::string &path)
{
    std::string extension = std::filesystem::path(path).extension().string();

    std::transform(extension.begin(), extension.end(), extension.begin(),
                   [](unsigned char character) { return static_cast<char>(std::tolower(character)); });
    return extension;
}

bool isFrameConversion(char character)
{
    return character == 'd' || character == 'i' || character == 'u';
}
} // namespace

void RayTracer::writePNGFile(const std::string &path, int width, int height, const std::vector<uint32_t> &pixels)
{
    if (width <= 0 || height <= 0)
        throw std::invalid_argument("PNG dimensions must be positive");

    const std::size_t pixelCount = static_cast<std::size_t>(width) * static_cast<std::size_t>(height);

    if (pixels.size() < pixelCount)
        throw std::invalid_argument("Not enough pixel data for the requested PNG size");

    if (stbi_write_png(path.c_str(), width, height, 4, pixels.data(), width * 4) == 0)
        throw std::runtime_error("Failed to write PNG file: " + path);
}

void RayTracer::writeImageFile(const std::string &path, int width, int height, const std::vector<uint32_t> &pixels)
{
    if (lowerExtension(path) == ".png")
        writePNGFile(path, width, height, pixels);
    else
        writePPMFile(path, width, height, pixels);
}

std::string RayTracer::formatFramePath(const std::string &pattern, int frame)
{
    if (frame < 0)
        throw std::invalid_argument("Frame index must not be negative");

    if (pattern.empty())
        return formatFramePath("frame-%04d.ppm", frame);

    bool hasConversion = false;

    for (std::size_t i = 0; i < pattern.size(); ++i)
    {
        if (pattern[i] != '%')
            continue;
        if (i + 1 < pattern.size() && pattern[i + 1] == '%')
        {
            ++i;
            continue;
        }

        if (hasConversion)
            throw std::invalid_argument("Frame path contains more than one format conversion");

        std::size_t cursor = i + 1;

        while (cursor < pattern.size() &&
               (pattern[cursor] == '-' || pattern[cursor] == '+' || pattern[cursor] == ' ' ||
                pattern[cursor] == '#' || pattern[cursor] == '0' || std::isdigit(static_cast<unsigned char>(pattern[cursor]))))
            ++cursor;

        if (cursor >= pattern.size() || !isFrameConversion(pattern[cursor]))
            throw std::invalid_argument("Frame path must contain one integer printf conversion");

        hasConversion = true;
        i = cursor;
    }

    if (hasConversion)
    {
        char buffer[4096];
        const int written = std::snprintf(buffer, sizeof(buffer), pattern.c_str(), frame);

        if (written < 0 || static_cast<std::size_t>(written) >= sizeof(buffer))
            throw std::runtime_error("Frame path is too long");
        return buffer;
    }

    const std::filesystem::path path(pattern);
    const std::string extension = path.extension().string();
    const std::string stem = path.stem().string();
    const std::string directory = path.parent_path().string();
    const std::string filename = stem + "-" + (frame < 10 ? "000" : frame < 100 ? "00" : frame < 1000 ? "0" : "") +
                                 std::to_string(frame) + (extension.empty() ? ".ppm" : extension);

    return directory.empty() ? filename : (std::filesystem::path(directory) / filename).string();
}
