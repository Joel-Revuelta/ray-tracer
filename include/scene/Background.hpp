/*
** EPITECH PROJECT, 2024
** raytracer
** File description:
** Background
*/

#ifndef BACKGROUND_HPP_
#define BACKGROUND_HPP_

#include "Color.hpp"
#include "Vec3D.hpp"
#include "render/Texture.hpp"
#include <algorithm>
#include <cmath>
#include <memory>

namespace RayTracer
{

/**
 * Optional background used when a camera or bounce ray misses all geometry.
 * An equirectangular environment texture takes precedence over the gradient.
 */
struct Background
{
    bool enabled = false;
    Color3D zenith{0.02f, 0.04f, 0.12f};
    Color3D horizon{0.12f, 0.16f, 0.28f};
    Color3D ground{0.01f, 0.01f, 0.02f};
    float intensity = 1.0f;
    std::shared_ptr<const Texture> environment;
    int environmentTextureIndex = -1;

    Color3D sample(const Vec3D &direction) const
    {
        const Vec3D unit = vec3DNormalize(direction);

        if (environment)
        {
            constexpr float PI = 3.14159265358979323846f;
            const float u = 0.5f + std::atan2(unit.getZ(), unit.getX()) / (2.0f * PI);
            const float v = std::acos(std::clamp(unit.getY(), -1.0f, 1.0f)) / PI;

            return environment->sampleClamped(u, v) * intensity;
        }

        const float vertical = std::clamp(unit.getY(), -1.0f, 1.0f);
        const float blend = std::sqrt(std::fabs(vertical));

        if (vertical >= 0.0f)
            return (horizon * (1.0f - blend) + zenith * blend) * intensity;
        return (horizon * (1.0f - blend) + ground * blend) * intensity;
    }
};

} // namespace RayTracer

#endif // BACKGROUND_HPP_
