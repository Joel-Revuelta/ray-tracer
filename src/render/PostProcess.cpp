/*
** EPITECH PROJECT, 2024
** raytracer
** File description:
** PostProcess
*/

#include "render/PostProcess.hpp"
#include "Color.hpp"
#include "Utils.hpp"
#include <cstddef>
#include <stdexcept>

using namespace RayTracer;

namespace
{
float colorLuminance(const Color3D &color)
{
    return 0.2126f * color.getX() + 0.7152f * color.getY() + 0.0722f * color.getZ();
}
} // namespace

void RayTracer::applyBloomCPU(std::vector<Color3D> &hdr, int width, int height,
                              const PostProcessSettings &settings)
{
    if (!settings.bloom || width <= 0 || height <= 0)
        return;

    const std::size_t count = static_cast<std::size_t>(width) * static_cast<std::size_t>(height);

    if (hdr.size() < count)
        throw std::invalid_argument("Not enough HDR pixel data for bloom");

    const int radius = std::clamp(settings.bloomRadius, 1, BLOOM_MAX_RADIUS);
    const std::array<float, BLOOM_MAX_RADIUS * 2 + 1> weights = bloomKernel(radius);
    std::vector<Color3D> bright(count);
    std::vector<Color3D> horizontal(count);
    std::vector<Color3D> vertical(count);

    for (std::size_t i = 0; i < count; ++i)
    {
        if (colorLuminance(hdr[i]) > settings.bloomThreshold)
            bright[i] = hdr[i];
    }

    for (int y = 0; y < height; ++y)
    {
        for (int x = 0; x < width; ++x)
        {
            Color3D value(0.0f);

            for (int offset = -radius; offset <= radius; ++offset)
            {
                const int sampleX = std::clamp(x + offset, 0, width - 1);
                value += bright[static_cast<std::size_t>(y) * width + static_cast<std::size_t>(sampleX)] *
                         weights[static_cast<std::size_t>(offset + BLOOM_MAX_RADIUS)];
            }

            horizontal[static_cast<std::size_t>(y) * width + static_cast<std::size_t>(x)] = value;
        }
    }

    for (int y = 0; y < height; ++y)
    {
        for (int x = 0; x < width; ++x)
        {
            Color3D value(0.0f);

            for (int offset = -radius; offset <= radius; ++offset)
            {
                const int sampleY = std::clamp(y + offset, 0, height - 1);
                value += horizontal[static_cast<std::size_t>(sampleY) * width + static_cast<std::size_t>(x)] *
                         weights[static_cast<std::size_t>(offset + BLOOM_MAX_RADIUS)];
            }

            vertical[static_cast<std::size_t>(y) * width + static_cast<std::size_t>(x)] = value;
        }
    }

    for (std::size_t i = 0; i < count; ++i)
        hdr[i] += vertical[i] * settings.bloomIntensity;
}

std::vector<std::uint32_t> RayTracer::postProcessToRGBA(std::vector<Color3D> hdr, int width, int height,
                                                        const PostProcessSettings &settings)
{
    if (width <= 0 || height <= 0)
        throw std::invalid_argument("Post-process dimensions must be positive");

    const std::size_t count = static_cast<std::size_t>(width) * static_cast<std::size_t>(height);

    if (hdr.size() < count)
        throw std::invalid_argument("Not enough HDR pixel data for post-processing");

    hdr.resize(count);
    applyBloomCPU(hdr, width, height, settings);

    std::vector<std::uint32_t> pixels(count);

    for (std::size_t i = 0; i < count; ++i)
        pixels[i] = Utils::convertToRGBA(Color4D(hdr[i], 1.0f), settings);
    return pixels;
}
