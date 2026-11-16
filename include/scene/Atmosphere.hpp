/*
** EPITECH PROJECT, 2024
** raytracer
** File description:
** Atmosphere
*/

#ifndef ATMOSPHERE_HPP_
#define ATMOSPHERE_HPP_

#include "Color.hpp"

namespace RayTracer
{

enum class AtmosphereType
{
    NONE,
    HOMOGENEOUS,
    HEIGHT
};

/**
 * Participating medium description.
 *
 * `density` is the extinction coefficient, `color` is the scattering albedo,
 * `anisotropy` is the Henyey-Greenstein g parameter and `maxDistance` clips
 * integration for rays that leave the scene. Height fog uses an exponential
 * density profile `density * exp(-heightFalloff * (y - groundY))`.
 */
struct Atmosphere
{
    AtmosphereType type = AtmosphereType::NONE;
    float density = 0.0f;
    Color3D color{1.0f, 1.0f, 1.0f};
    float anisotropy = 0.0f;
    float groundY = 0.0f;
    float heightFalloff = 0.0f;
    float maxDistance = 100.0f;
};

} // namespace RayTracer

#endif // ATMOSPHERE_HPP_
