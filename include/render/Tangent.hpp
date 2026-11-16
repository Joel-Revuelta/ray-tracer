/*
** EPITECH PROJECT, 2024
** raytracer
** File description:
** Tangent
*/

#ifndef TANGENT_HPP_
#define TANGENT_HPP_

#include "Vec3D.hpp"
#include <cmath>

namespace RayTracer
{

/** Returns a deterministic unit tangent for a normal, or zero if degenerate. */
inline Vec3D stableTangent(const Vec3D &normal)
{
    const Vec3D axis = std::fabs(normal.getY()) < 0.999f ? Vec3D(0.0f, 1.0f, 0.0f) : Vec3D(1.0f, 0.0f, 0.0f);
    Vec3D tangent = vec3DCross(axis, normal);
    const float lengthSquared = tangent.lengthSquared();

    if (!std::isfinite(lengthSquared) || lengthSquared < 1e-12f)
        return Vec3D(0.0f);
    return tangent / std::sqrt(lengthSquared);
}

/** Projects a tangent onto the plane perpendicular to the normal and normalizes it. */
inline Vec3D orthonormalizeTangent(const Vec3D &tangent, const Vec3D &normal)
{
    Vec3D result = tangent - normal * vec3DDot(normal, tangent);
    const float lengthSquared = result.lengthSquared();

    if (!std::isfinite(lengthSquared) || lengthSquared < 1e-12f)
        return stableTangent(normal);
    return result / std::sqrt(lengthSquared);
}

} // namespace RayTracer

#endif // TANGENT_HPP_
