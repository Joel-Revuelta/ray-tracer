/*
** EPITECH PROJECT, 2024
** raytracer
** File description:
** PrimitiveData
*/

#ifndef PRIMITIVE_DATA_HPP_
#define PRIMITIVE_DATA_HPP_

#include "Vec3D.hpp"
#include <cstdint>

namespace RayTracer
{

enum class PrimitiveType
{
    NONE,
    SPHERE,
    PLANE,
    CYLINDER,
    CONE
};

/**
 * Renderer-neutral description of a primitive.
 *
 * `center` is the sphere centre, the cylinder bottom-cap centre or the cone
 * apex. `point`/`normal` are only used by planes. `axis` is only used by
 * cylinders.
 */
struct PrimitiveData
{
    PrimitiveType type = PrimitiveType::NONE;
    uint32_t materialIndex = 0;
    Vec3D center;
    Vec3D point;
    Vec3D normal;
    Vec3D axis{0.0f, 1.0f, 0.0f};
    float radius = 0.0f;
    float height = 0.0f;
    bool coneBaseAbove = false;
};

} // namespace RayTracer

#endif // PRIMITIVE_DATA_HPP_
