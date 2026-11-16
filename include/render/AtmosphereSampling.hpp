/*
** EPITECH PROJECT, 2024
** raytracer
** File description:
** AtmosphereSampling
*/

#ifndef ATMOSPHERE_SAMPLING_HPP_
#define ATMOSPHERE_SAMPLING_HPP_

#include "Tangent.hpp"
#include "Utils.hpp"
#include "Vec3D.hpp"
#include "scene/Atmosphere.hpp"
#include <algorithm>
#include <cmath>
#include <limits>

namespace RayTracer
{

inline bool atmosphereActive(const Atmosphere &atmosphere)
{
    return atmosphere.type != AtmosphereType::NONE && atmosphere.density > 0.0f;
}

/**
 * Optical depth of an exponential height-fog medium along a ray segment.
 * `direction` must be a unit vector and `distance` is a world-space length.
 */
inline float atmosphereOpticalDepth(const Atmosphere &atmosphere, const Vec3D &origin, const Vec3D &direction,
                                    float distance)
{
    if (!(distance > 0.0f))
        return 0.0f;

    if (atmosphere.type == AtmosphereType::HOMOGENEOUS)
        return atmosphere.density * distance;

    const float falloff = atmosphere.heightFalloff;
    const float base = atmosphere.density * std::exp(-falloff * (origin.getY() - atmosphere.groundY));
    const float vertical = falloff * direction.getY();

    if (!std::isfinite(base) || base < 0.0f)
        return std::numeric_limits<float>::infinity();
    if (std::fabs(vertical) < 1e-5f)
        return base * distance;

    const float depth = -std::expm1(-vertical * distance) / vertical;

    // A downward exponential profile can overflow. That means the medium is
    // optically infinite, not transparent.
    if (std::isinf(depth))
        return depth > 0.0f ? std::numeric_limits<float>::infinity() : 0.0f;
    return std::isfinite(depth) && depth > 0.0f ? base * depth : 0.0f;
}

inline float atmosphereTransmittance(const Atmosphere &atmosphere, const Vec3D &origin, const Vec3D &direction,
                                     float distance)
{
    if (!atmosphereActive(atmosphere))
        return 1.0f;

    const float depth = atmosphereOpticalDepth(atmosphere, origin, direction,
                                               std::min(distance, atmosphere.maxDistance));

    if (!(depth > 0.0f))
        return 1.0f;
    if (!std::isfinite(depth))
        return 0.0f;
    return std::exp(-depth);
}

/**
 * Samples a scattering distance for the medium. Returns false when the height
 * fog distribution has no finite scattering event (the sample lies beyond the
 * optical-depth limit of an upward ray); callers should then treat the segment
 * as unscattered and attenuate by its regular transmittance.
 */
inline bool sampleAtmosphereDistance(const Atmosphere &atmosphere, const Vec3D &origin, const Vec3D &direction,
                                     float sample, float &distance)
{
    if (!atmosphereActive(atmosphere))
        return false;

    const float clamped = std::clamp(sample, 0.0f, 0.99999994f);
    const float target = -std::log(1.0f - clamped);

    if (atmosphere.type == AtmosphereType::HOMOGENEOUS)
    {
        distance = target / atmosphere.density;
        return std::isfinite(distance) && distance >= 0.0f;
    }

    const float falloff = atmosphere.heightFalloff;
    const float base = atmosphere.density * std::exp(-falloff * (origin.getY() - atmosphere.groundY));
    const float vertical = falloff * direction.getY();

    if (!(base > 0.0f) || !std::isfinite(base))
        return false;
    if (std::fabs(vertical) < 1e-5f)
    {
        distance = target / base;
        return std::isfinite(distance) && distance >= 0.0f;
    }

    // For upward rays the optical depth saturates at base / vertical, so a
    // sample beyond that limit has no finite event.
    if (vertical > 0.0f && target >= base / vertical)
        return false;

    const float remaining = 1.0f - target * vertical / base;

    if (!(remaining > 0.0f))
        return false;

    distance = -std::log(remaining) / vertical;
    return std::isfinite(distance) && distance >= 0.0f;
}

inline float henyeyGreensteinPhase(float cosine, float anisotropy)
{
    const float g = std::clamp(anisotropy, -0.999f, 0.999f);
    const float denominator = std::max(1.0f + g * g - 2.0f * g * cosine, 1e-6f);
    const float root = std::sqrt(denominator);

    return (1.0f - g * g) / (4.0f * static_cast<float>(Utils::PI) * denominator * root);
}

inline Vec3D sampleHenyeyGreenstein(const Vec3D &direction, float anisotropy, float u1, float u2)
{
    const float g = std::clamp(anisotropy, -0.999f, 0.999f);
    const float uniform = std::clamp(u1, 0.0f, 0.99999994f);
    float cosine;

    if (std::fabs(g) < 1e-3f)
        cosine = 1.0f - 2.0f * uniform;
    else
    {
        const float root = (1.0f - g * g) / (1.0f + g - 2.0f * g * uniform);

        cosine = (1.0f + g * g - root * root) / (2.0f * g);
    }

    cosine = std::clamp(cosine, -1.0f, 1.0f);

    const float sine = std::sqrt(std::max(0.0f, 1.0f - cosine * cosine));
    const float phi = 2.0f * static_cast<float>(Utils::PI) * std::clamp(u2, 0.0f, 0.99999994f);
    Vec3D tangent = stableTangent(direction);

    if (tangent.lengthSquared() < 1e-12f)
        tangent = Vec3D(1.0f, 0.0f, 0.0f);

    const Vec3D bitangent = vec3DCross(direction, tangent);
    const Vec3D scattered = tangent * (std::cos(phi) * sine) + bitangent * (std::sin(phi) * sine) +
                            direction * cosine;

    if (scattered.lengthSquared() < 1e-12f)
        return direction;
    return vec3DNormalize(scattered);
}

} // namespace RayTracer

#endif // ATMOSPHERE_SAMPLING_HPP_
