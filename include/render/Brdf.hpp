/*
** EPITECH PROJECT, 2024
** raytracer
** File description:
** Brdf
*/

#ifndef BRDF_HPP_
#define BRDF_HPP_

#include "Color.hpp"
#include "Utils.hpp"
#include "Vec3D.hpp"
#include <algorithm>
#include <cmath>

namespace RayTracer
{

constexpr float PI_F = 3.14159265358979323846f;

inline float smithGGX(float ndot, float alpha)
{
    const float a2 = alpha * alpha;

    return (2.0f * ndot) / (ndot + std::sqrt(a2 + (1.0f - a2) * ndot * ndot));
}

inline Color3D fresnelSchlick(const Color3D &f0, float cosTheta)
{
    const float factor = std::pow(std::clamp(1.0f - cosTheta, 0.0f, 1.0f), 5.0f);

    return f0 + (Color3D(1.0f, 1.0f, 1.0f) - f0) * factor;
}

inline float blinnShininess(float roughness)
{
    if (roughness <= 1e-3f)
        return 64.0f;

    const float alpha = std::max(roughness, 1e-3f);
    const float exponent = 2.0f / (alpha * alpha * alpha * alpha) - 2.0f;

    return std::clamp(exponent, 1.0f, 4096.0f);
}

inline Vec3D sampleGGXHalfVector(const Vec3D &normal, float roughness, uint32_t &seed)
{
    const float alpha = std::max(roughness * roughness, 1e-4f);
    const float u1 = Utils::numericGeneratePseudoRandom<float>(seed, 0.0f, 1.0f);
    const float u2 = Utils::numericGeneratePseudoRandom<float>(seed, 0.0f, 1.0f);
    const float phi = 2.0f * PI_F * u1;
    const float cosTheta = std::sqrt((1.0f - u2) / (1.0f + (alpha * alpha - 1.0f) * u2));
    const float sinTheta = std::sqrt(std::max(0.0f, 1.0f - cosTheta * cosTheta));

    const Vec3D tangent =
        std::fabs(normal.getX()) < 0.999f
            ? vec3DNormalize(vec3DCross(Vec3D(1.0f, 0.0f, 0.0f), normal))
            : vec3DNormalize(vec3DCross(Vec3D(0.0f, 1.0f, 0.0f), normal));
    const Vec3D bitangent = vec3DCross(normal, tangent);

    return vec3DNormalize(tangent * (std::cos(phi) * sinTheta) + bitangent * (std::sin(phi) * sinTheta) +
                          normal * cosTheta);
}

/**
 * Samples a metallic reflection. `viewDirection` points from the hit point
 * towards the viewer.
 */
inline bool sampleMetallic(const Vec3D &viewDirection, const Vec3D &normal, const Color3D &f0, float roughness,
                           uint32_t &seed, Vec3D &direction, Color3D &weight)
{
    if (roughness <= 1e-3f)
    {
        direction = vec3DNormalize(reflect(-viewDirection, normal));
        weight = f0;
        return !vec3DIsNearZero(direction);
    }

    const Vec3D halfVector = sampleGGXHalfVector(normal, roughness, seed);

    direction = vec3DNormalize(reflect(-viewDirection, halfVector));

    const float ndotL = vec3DDot(normal, direction);
    const float ndotV = std::max(vec3DDot(normal, viewDirection), 1e-4f);
    const float ndotH = std::max(vec3DDot(normal, halfVector), 1e-4f);
    const float vdotH = std::max(vec3DDot(viewDirection, halfVector), 1e-4f);

    if (ndotL <= 0.0f)
        return false;

    const float alpha = std::max(roughness * roughness, 1e-4f);
    const Color3D fresnel = fresnelSchlick(f0, vdotH);
    const float geometry = smithGGX(ndotV, alpha) * smithGGX(ndotL, alpha);
    const float factor = std::clamp(geometry * vdotH / (ndotV * ndotH), 0.0f, 1.0f);

    weight = fresnel * factor;
    return !vec3DIsNearZero(weight);
}

} // namespace RayTracer

#endif // BRDF_HPP_
