/*
** EPITECH PROJECT, 2024
** raytracer
** File description:
** PostProcess
*/

#ifndef POST_PROCESS_HPP_
#define POST_PROCESS_HPP_

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <vector>

namespace RayTracer
{

class Vec3D;

enum class ToneMapping : int
{
    NONE = 0,
    REINHARD = 1,
    ACES = 2
};

constexpr int BLOOM_MAX_RADIUS = 16;

struct PostProcessSettings
{
    float exposure = 1.0f;
    ToneMapping toneMapping = ToneMapping::NONE;
    bool srgb = true;
    bool bloom = false;
    float bloomThreshold = 1.0f;
    float bloomIntensity = 0.15f;
    int bloomRadius = 8;
};

inline std::array<float, BLOOM_MAX_RADIUS * 2 + 1> bloomKernel(int radius)
{
    std::array<float, BLOOM_MAX_RADIUS * 2 + 1> weights{};
    const int clampedRadius = std::clamp(radius, 1, BLOOM_MAX_RADIUS);
    const float sigma = std::max(1.0f, static_cast<float>(clampedRadius) * 0.5f);
    const float denominator = 2.0f * sigma * sigma;
    float total = 0.0f;

    for (int offset = -clampedRadius; offset <= clampedRadius; ++offset)
    {
        const float weight = std::exp(-static_cast<float>(offset * offset) / denominator);

        weights[static_cast<std::size_t>(offset + BLOOM_MAX_RADIUS)] = weight;
        total += weight;
    }

    for (float &weight : weights)
        weight /= total;
    return weights;
}

inline float applyToneMapping(float linear, ToneMapping mode)
{
    const float exposed = std::max(linear, 0.0f);

    switch (mode)
    {
    case ToneMapping::NONE:
        return exposed;
    case ToneMapping::REINHARD:
        return exposed / (1.0f + exposed);
    case ToneMapping::ACES:
    {
        constexpr float a = 2.51f;
        constexpr float b = 0.03f;
        constexpr float c = 2.43f;
        constexpr float d = 0.59f;
        constexpr float e = 0.14f;

        return (exposed * (a * exposed + b)) / (exposed * (c * exposed + d) + e);
    }
    default:
        return exposed;
    }
}

inline float linearToSRGB(float linear)
{
    const float value = std::clamp(linear, 0.0f, 1.0f);

    if (value <= 0.0031308f)
        return 12.92f * value;
    return 1.055f * std::pow(value, 1.0f / 2.4f) - 0.055f;
}

inline std::uint8_t postProcessToByte(float linear, const PostProcessSettings &settings, bool alpha = false)
{
    if (!std::isfinite(linear))
        return 0;

    if (alpha)
        return static_cast<std::uint8_t>(std::lround(std::clamp(linear, 0.0f, 1.0f) * 255.0f));

    float value = applyToneMapping(linear * settings.exposure, settings.toneMapping);

    if (settings.srgb)
        value = linearToSRGB(value);

    return static_cast<std::uint8_t>(std::lround(std::clamp(value, 0.0f, 1.0f) * 255.0f));
}

/** Applies the CPU bright-pass and separable Gaussian bloom to linear HDR data. */
void applyBloomCPU(std::vector<Vec3D> &hdr, int width, int height, const PostProcessSettings &settings);

/** Post-processes linear HDR pixels into RGBA bytes, including optional bloom. */
std::vector<std::uint32_t> postProcessToRGBA(std::vector<Vec3D> hdr, int width, int height,
                                             const PostProcessSettings &settings);

} // namespace RayTracer

#endif // POST_PROCESS_HPP_
