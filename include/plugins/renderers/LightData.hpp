/*
** EPITECH PROJECT, 2024
** raytracer
** File description:
** LightData
*/

#ifndef LIGHT_DATA_HPP_
#define LIGHT_DATA_HPP_

#include "Color.hpp"

namespace RayTracer
{

enum class LightType
{
    NONE,
    AMBIENT,
    DIRECTIONAL,
    POINT
};

/**
 * Renderer-neutral description of a light source.
 *
 * `position` is used by point lights, `direction` by directional lights and
 * the attenuation fields only by point lights.
 */
struct LightData
{
    LightType type = LightType::NONE;
    Color3D color;
    Vec3D position;
    Vec3D direction;
    float intensity = 0.0f;
    float attenuationConstant = 1.0f;
    float attenuationLinear = 0.0f;
    float attenuationQuadratic = 0.0f;
};

} // namespace RayTracer

#endif // LIGHT_DATA_HPP_
