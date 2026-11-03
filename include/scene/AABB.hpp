/*
** EPITECH PROJECT, 2024
** raytracer
** File description:
** AABB
*/

#ifndef AABB_HPP_
#define AABB_HPP_

#include "Ray.hpp"
#include "Vec3D.hpp"
#include <algorithm>
#include <cmath>
#include <limits>

namespace RayTracer
{

/**
 * Axis-aligned bounding box used by the CPU BVH.
 *
 * The default box is empty: min is +infinity and max is -infinity.
 */
struct AABB
{
    Vec3D min{std::numeric_limits<float>::infinity(), std::numeric_limits<float>::infinity(),
              std::numeric_limits<float>::infinity()};
    Vec3D max{-std::numeric_limits<float>::infinity(), -std::numeric_limits<float>::infinity(),
              -std::numeric_limits<float>::infinity()};

    bool valid(void) const
    {
        return min.getX() <= max.getX() && min.getY() <= max.getY() && min.getZ() <= max.getZ();
    }

    void expand(const Vec3D &point)
    {
        min.setX(std::min(min.getX(), point.getX()));
        min.setY(std::min(min.getY(), point.getY()));
        min.setZ(std::min(min.getZ(), point.getZ()));
        max.setX(std::max(max.getX(), point.getX()));
        max.setY(std::max(max.getY(), point.getY()));
        max.setZ(std::max(max.getZ(), point.getZ()));
    }

    void expand(const AABB &other)
    {
        if (!other.valid())
            return;
        expand(other.min);
        expand(other.max);
    }

    Vec3D centroid(void) const
    {
        return (min + max) * 0.5f;
    }

    Vec3D extent(void) const
    {
        return max - min;
    }

    float surfaceArea(void) const
    {
        if (!valid())
            return 0.0f;

        const Vec3D size = extent();

        return 2.0f * (size.getX() * size.getY() + size.getY() * size.getZ() + size.getZ() * size.getX());
    }

    /**
     * Returns the distance at which the ray enters the box, or infinity when it
     * misses. Used to visit the nearest BVH child first.
     */
    float entryDistance(const Ray &ray) const
    {
        if (!valid())
            return std::numeric_limits<float>::infinity();

        const Vec3D origin = ray.getOrigin();
        const Vec3D direction = ray.getDirection();
        float nearDistance = 0.0f;
        float farDistance = std::numeric_limits<float>::infinity();

        for (int axis = 0; axis < 3; ++axis)
        {
            const float axisOrigin = origin[axis];
            const float axisDirection = direction[axis];

            if (std::fabs(axisDirection) < Ray::DIRECTION_EPSILON)
            {
                if (axisOrigin < min[axis] || axisOrigin > max[axis])
                    return std::numeric_limits<float>::infinity();
                continue;
            }

            float first = (min[axis] - axisOrigin) / axisDirection;
            float second = (max[axis] - axisOrigin) / axisDirection;

            if (first > second)
                std::swap(first, second);

            nearDistance = std::max(nearDistance, first);
            farDistance = std::min(farDistance, second);

            if (nearDistance > farDistance)
                return std::numeric_limits<float>::infinity();
        }

        return nearDistance;
    }

    bool intersect(const Ray &ray, float maxDistance) const
    {
        const Vec3D origin = ray.getOrigin();
        const Vec3D direction = ray.getDirection();
        float nearDistance = 0.0f;
        float farDistance = maxDistance;

        for (int axis = 0; axis < 3; ++axis)
        {
            const float axisOrigin = origin[axis];
            const float axisDirection = direction[axis];

            if (std::fabs(axisDirection) < Ray::DIRECTION_EPSILON)
            {
                if (axisOrigin < min[axis] || axisOrigin > max[axis])
                    return false;
                continue;
            }

            float first = (min[axis] - axisOrigin) / axisDirection;
            float second = (max[axis] - axisOrigin) / axisDirection;

            if (first > second)
                std::swap(first, second);

            nearDistance = std::max(nearDistance, first);
            farDistance = std::min(farDistance, second);

            if (nearDistance > farDistance)
                return false;
        }

        return true;
    }
};

} // namespace RayTracer

#endif // AABB_HPP_
