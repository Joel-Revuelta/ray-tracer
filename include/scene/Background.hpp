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
#include <algorithm>
#include <cmath>

namespace RayTracer
{

/**
 * Optional gradient background used when a camera or bounce ray misses all
 * geometry. The gradient is sampled from the ray direction.
 */
struct Background
{
    bool enabled = false;
    Color3D zenith{0.02f, 0.04f, 0.12f};
    Color3D horizon{0.12f, 0.16f, 0.28f};
    Color3D ground{0.01f, 0.01f, 0.02f};
    float intensity = 1.0f;

    Color3D sample(const Vec3D &direction) const
    {
        const Vec3D unit = vec3DNormalize(direction);
        const float vertical = std::clamp(unit.getY(), -1.0f, 1.0f);
        const float blend = std::sqrt(std::fabs(vertical));

        if (vertical >= 0.0f)
            return (horizon * (1.0f - blend) + zenith * blend) * intensity;
        return (horizon * (1.0f - blend) + ground * blend) * intensity;
    }
};

} // namespace RayTracer

#endif // BACKGROUND_HPP_
