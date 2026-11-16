/*
** EPITECH PROJECT, 2024
** raytracer
** File description:
** SamplingMath.hpp
*/

#ifndef SAMPLING_MATH_HPP_
#define SAMPLING_MATH_HPP_

#include <cmath>
#include <math.h>

#if defined(__CUDACC__)
#define RAYTRACER_SAMPLING_HD __host__ __device__
#else
#define RAYTRACER_SAMPLING_HD
#endif

namespace RayTracer
{

/** Shared scalar sampling formulas used by both renderers. */
RAYTRACER_SAMPLING_HD inline float clamp01Sampling(float value)
{
    return value < 0.0f ? 0.0f : (value > 1.0f ? 1.0f : value);
}

RAYTRACER_SAMPLING_HD inline float smithGGXSampling(float ndot, float alpha)
{
    const float a2 = alpha * alpha;

    return (2.0f * ndot) / (ndot + sqrtf(a2 + (1.0f - a2) * ndot * ndot));
}

RAYTRACER_SAMPLING_HD inline float schlickFactorSampling(float cosTheta)
{
    const float factor = 1.0f - clamp01Sampling(cosTheta);

    return factor * factor * factor * factor * factor;
}

RAYTRACER_SAMPLING_HD inline float blinnShininessSampling(float roughness)
{
    if (roughness <= 1e-3f)
        return 64.0f;

    const float alpha = fmaxf(roughness, 1e-3f);
    const float exponent = 2.0f / (alpha * alpha * alpha * alpha) - 2.0f;

    return fminf(fmaxf(exponent, 1.0f), 4096.0f);
}

RAYTRACER_SAMPLING_HD inline float henyeyGreensteinPhaseSampling(float cosine, float anisotropy)
{
    const float g = fminf(fmaxf(anisotropy, -0.999f), 0.999f);
    const float denominator = fmaxf(1.0f + g * g - 2.0f * g * cosine, 1e-6f);

    return (1.0f - g * g) / (4.0f * 3.14159265358979323846f * denominator * sqrtf(denominator));
}

RAYTRACER_SAMPLING_HD inline float powerHeuristicSampling(float first, float second)
{
    const float firstSquared = first * first;
    const float secondSquared = second * second;
    const float total = firstSquared + secondSquared;

    return total > 0.0f ? firstSquared / total : 0.0f;
}

} // namespace RayTracer

#endif // SAMPLING_MATH_HPP_
