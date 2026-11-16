/*
** EPITECH PROJECT, 2024
** raytracer
** File description:
** ImageWriter
*/

#include "render/ImageWriter.hpp"
#include "render/TemporaryPath.hpp"
#include "plugins/renderers/PPM.hpp"
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"
#include <algorithm>
#include <cctype>
#include <cstdio>
#include <filesystem>
#include <limits>
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
    if (width > std::numeric_limits<int>::max() / 4)
        throw std::invalid_argument("PNG width is too large for the image writer");

    if (stbi_write_png(path.c_str(), width, height, 4, pixels.data(), width * 4) == 0)
        throw std::runtime_error("Failed to write PNG file: " + path);
}

bool RayTracer::isImageExtension(const std::string &path)
{
    const std::string extension = lowerExtension(path);

    return extension == ".ppm" || extension == ".png";
}

void RayTracer::writeImageFile(const std::string &path, int width, int height, const std::vector<uint32_t> &pixels)
{
    const std::string extension = lowerExtension(path);

    if (extension == ".png")
        writePNGFile(path, width, height, pixels);
    else if (extension == ".ppm")
        writePPMFile(path, width, height, pixels);
    else
        throw std::invalid_argument("Unsupported output extension '" + extension +
                                    "'; expected .ppm or .png");
}

void RayTracer::writeImageFileAtomic(const std::string &path, int width, int height,
                                     const std::vector<uint32_t> &pixels)
{
    if (path.empty())
        throw std::invalid_argument("Output path must not be empty");

    const std::filesystem::path destination(path);
    const std::filesystem::path temporary(makeTemporaryPath(path));

    try
    {
        writeImageFile(temporary.string(), width, height, pixels);
    }
    catch (...)
    {
        std::error_code cleanupError;

        std::filesystem::remove(temporary, cleanupError);
        throw;
    }

    std::error_code error;

    std::filesystem::rename(temporary, destination, error);
    if (error)
    {
        std::filesystem::remove(temporary, error);
        throw std::runtime_error("Failed to finalize output file: " + path);
    }
}

bool RayTracer::frameFileExists(const std::string &path)
{
    std::error_code error;
    const bool exists = std::filesystem::exists(path, error);

    if (error || !exists || !std::filesystem::is_regular_file(path, error) || error)
        return false;

    const std::uintmax_t size = std::filesystem::file_size(path, error);

    return !error && size > 0;
}

std::string RayTracer::formatFramePath(const std::string &pattern, int frame)
{
    if (frame < 0)
        throw std::invalid_argument("Frame index must not be negative");

    if (pattern.empty())
        return formatFramePath("frame-%04d.ppm", frame);

    bool hasConversion = false;
    char conversion = '\0';

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
        conversion = pattern[cursor];
        i = cursor;
    }

    if (hasConversion)
    {
        char buffer[4096];
        const int written =
            conversion == 'u'
                ? std::snprintf(buffer, sizeof(buffer), pattern.c_str(), static_cast<unsigned int>(frame))
                : std::snprintf(buffer, sizeof(buffer), pattern.c_str(), frame);

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
