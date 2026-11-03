/*
** EPITECH PROJECT, 2024
** raytracer
** File description:
** PrimitiveBounds
*/

#ifndef PRIMITIVE_BOUNDS_HPP_
#define PRIMITIVE_BOUNDS_HPP_

#include "AABB.hpp"
#include "plugins/renderers/PrimitiveData.hpp"

namespace RayTracer
{

/**
 * Computes a conservative host-side bounding box for a primitive description.
 * Returns false for planes and unsupported primitive kinds, which must be
 * treated as unbounded by acceleration structures.
 */
inline bool computePrimitiveBounds(const PrimitiveData &data, AABB &bounds)
{
    const Vec3D allRadius(data.radius);

    switch (data.type)
    {
    case PrimitiveType::SPHERE:
        bounds.expand(data.center - allRadius);
        bounds.expand(data.center + allRadius);
        return bounds.valid();
    case PrimitiveType::CYLINDER:
    {
        const Vec3D axis = vec3DNormalize(data.axis);
        const Vec3D top = data.center + axis * data.height;

        bounds.expand(data.center - allRadius);
        bounds.expand(data.center + allRadius);
        bounds.expand(top - allRadius);
        bounds.expand(top + allRadius);
        return bounds.valid();
    }
    case PrimitiveType::CONE:
    {
        const Vec3D base =
            data.center + Vec3D(0.0f, data.coneBaseAbove ? data.height : -data.height, 0.0f);

        bounds.expand(data.center);
        bounds.expand(base - allRadius);
        bounds.expand(base + allRadius);
        return bounds.valid();
    }
    case PrimitiveType::PLANE:
    case PrimitiveType::NONE:
    default:
        return false;
    }
}

} // namespace RayTracer

#endif // PRIMITIVE_BOUNDS_HPP_
