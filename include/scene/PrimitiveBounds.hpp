/*
** EPITECH PROJECT, 2024
** raytracer
** File description:
** PrimitiveBounds
*/

#ifndef PRIMITIVE_BOUNDS_HPP_
#define PRIMITIVE_BOUNDS_HPP_

#include "AABB.hpp"
#include "Transform.hpp"
#include "plugins/renderers/PrimitiveData.hpp"

namespace RayTracer
{

/**
 * Computes a conservative world-space bounding box for a primitive
 * description. Returns false for planes and unsupported primitive kinds, which
 * must be treated as unbounded by acceleration structures.
 */
inline bool computePrimitiveBounds(const PrimitiveData &data, AABB &bounds)
{
    const Vec3D allRadius(data.radius);
    AABB local;

    switch (data.type)
    {
    case PrimitiveType::SPHERE:
        local.expand(data.center - allRadius);
        local.expand(data.center + allRadius);
        break;
    case PrimitiveType::CYLINDER:
    {
        const Vec3D axis = vec3DNormalize(data.axis);
        const Vec3D top = data.center + axis * data.height;

        local.expand(data.center - allRadius);
        local.expand(data.center + allRadius);
        local.expand(top - allRadius);
        local.expand(top + allRadius);
        break;
    }
    case PrimitiveType::CONE:
    {
        const Vec3D base =
            data.center + Vec3D(0.0f, data.coneBaseAbove ? data.height : -data.height, 0.0f);

        local.expand(data.center);
        local.expand(base - allRadius);
        local.expand(base + allRadius);
        break;
    }
    case PrimitiveType::PLANE:
    case PrimitiveType::NONE:
    default:
        return false;
    }

    if (!local.valid())
        return false;

    bounds = transformBounds(data.transform, local);
    return bounds.valid();
}

} // namespace RayTracer

#endif // PRIMITIVE_BOUNDS_HPP_
