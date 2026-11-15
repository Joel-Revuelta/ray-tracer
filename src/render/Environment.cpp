/*
** EPITECH PROJECT, 2024
** raytracer
** File description:
** Environment
*/

#include "render/Environment.hpp"
#include "render/Texture.hpp"
#include "Utils.hpp"
#include <algorithm>
#include <cmath>
#include <limits>

using namespace RayTracer;

namespace
{
constexpr float PI = 3.14159265358979323846f;

float random01(uint32_t &seed)
{
    return Utils::numericGeneratePseudoRandom<float>(seed, 0.0f, 1.0f);
}

int sampleCdf(const float *cdf, int count, float value)
{
    if (count <= 0)
        return -1;

    const float *end = cdf + count;
    const float *found = std::lower_bound(cdf, end, value);

    if (found == end)
        found = end - 1;
    return static_cast<int>(found - cdf);
}

float texelLuminance(const Texture &texture, int index)
{
    const Color3D &pixel = texture.getPixels()[static_cast<std::size_t>(index)];

    return std::max(0.0f, environmentLuminance(pixel));
}
} // namespace

float RayTracer::environmentLuminance(const Color3D &color)
{
    return 0.2126f * color.getX() + 0.7152f * color.getY() + 0.0722f * color.getZ();
}

std::shared_ptr<const EnvironmentDistribution> RayTracer::buildEnvironmentDistribution(const Texture &texture)
{
    auto distribution = std::make_shared<EnvironmentDistribution>();

    distribution->width = texture.getWidth();
    distribution->height = texture.getHeight();
    distribution->conditionalCdf.assign(static_cast<std::size_t>(distribution->width) * distribution->height, 0.0f);
    distribution->marginalCdf.assign(static_cast<std::size_t>(distribution->height), 0.0f);

    if (distribution->width <= 0 || distribution->height <= 0)
        return distribution;

    std::vector<float> rowWeights(static_cast<std::size_t>(distribution->height), 0.0f);

    for (int y = 0; y < distribution->height; ++y)
    {
        const float theta = static_cast<float>(PI) * (static_cast<float>(y) + 0.5f) /
                            static_cast<float>(distribution->height);
        const float sinTheta = std::sin(theta);
        float rowSum = 0.0f;

        for (int x = 0; x < distribution->width; ++x)
        {
            const int index = y * distribution->width + x;
            const float weight = texelLuminance(texture, index);

            rowSum += weight;
            distribution->conditionalCdf[static_cast<std::size_t>(index)] = rowSum;
        }

        if (rowSum > 0.0f)
        {
            float *row = distribution->conditionalCdf.data() + static_cast<std::size_t>(y) * distribution->width;

            for (int x = 0; x < distribution->width; ++x)
                row[x] /= rowSum;
        }

        rowWeights[static_cast<std::size_t>(y)] = rowSum * sinTheta;
    }

    float total = 0.0f;

    for (float weight : rowWeights)
        total += weight;

    distribution->total = total;

    if (total <= 0.0f)
        return distribution;

    float running = 0.0f;

    for (int y = 0; y < distribution->height; ++y)
    {
        running += rowWeights[static_cast<std::size_t>(y)] / total;
        distribution->marginalCdf[static_cast<std::size_t>(y)] = running;
    }

    return distribution;
}

EnvironmentSample RayTracer::sampleEnvironment(const Texture &texture, const EnvironmentDistribution &distribution,
                                               uint32_t &seed)
{
    EnvironmentSample sample;

    if (distribution.width <= 0 || distribution.height <= 0 || distribution.total <= 0.0f)
        return sample;

    const float rowValue = random01(seed);
    const int y = sampleCdf(distribution.marginalCdf.data(), distribution.height, rowValue);

    if (y < 0 || y >= distribution.height)
        return sample;

    const float *row = distribution.conditionalCdf.data() + static_cast<std::size_t>(y) * distribution.width;
    const int x = sampleCdf(row, distribution.width, random01(seed));

    if (x < 0 || x >= distribution.width)
        return sample;

    const int index = y * distribution.width + x;
    const float texelWeight = texelLuminance(texture, index);

    if (texelWeight <= 0.0f)
        return sample;

    const float u = (static_cast<float>(x) + 0.5f) / static_cast<float>(distribution.width);
    const float v = (static_cast<float>(y) + 0.5f) / static_cast<float>(distribution.height);

    sample.direction = Vec3D(std::sin(PI * v) * std::cos(2.0f * PI * (u - 0.5f)),
                             std::cos(PI * v),
                             std::sin(PI * v) * std::sin(2.0f * PI * (u - 0.5f)));
    sample.radiance = texture.sampleClamped(u, v);
    sample.pdf = texelWeight * static_cast<float>(distribution.width) * static_cast<float>(distribution.height) /
                 (distribution.total * 2.0f * PI * PI);
    sample.valid = true;
    return sample;
}

float RayTracer::environmentPdf(const Texture &texture, const EnvironmentDistribution &distribution,
                                const Vec3D &direction)
{
    if (distribution.width <= 0 || distribution.height <= 0 || distribution.total <= 0.0f)
        return 0.0f;

    const Vec3D unit = vec3DNormalize(direction);

    if (vec3DIsNearZero(unit))
        return 0.0f;

    const float u = 0.5f + std::atan2(unit.getZ(), unit.getX()) / (2.0f * PI);
    const float v = std::acos(std::clamp(unit.getY(), -1.0f, 1.0f)) / PI;
    const int x = std::clamp(static_cast<int>(std::floor(u * distribution.width)), 0, distribution.width - 1);
    const int y = std::clamp(static_cast<int>(std::floor(v * distribution.height)), 0, distribution.height - 1);
    const float texelWeight = texelLuminance(texture, y * distribution.width + x);

    if (texelWeight <= 0.0f)
        return 0.0f;

    return texelWeight * static_cast<float>(distribution.width) * static_cast<float>(distribution.height) /
           (distribution.total * 2.0f * PI * PI);
}
