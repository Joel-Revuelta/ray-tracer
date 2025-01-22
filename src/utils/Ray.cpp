/*
** EPITECH PROJECT, 2023
** raytracer
** File description:
** Ray.cpp
*/

#include "Ray.hpp"

using namespace RayTracer;

Ray::Ray(void) : _origin(Point3D(0, 0, 0)), _direction(Vec3D(0, 0, 0))
{
}

Ray::Ray(const Point3D &origin, const Vec3D &direction) : _origin(origin), _direction(direction)
{
}

Ray::~Ray(void)
{
}

/**
 * Returns the point on the ray at a given distance.
 *
 * @param t The distance.
 * @return The point on the ray at the given distance.
 */
Point3D Ray::at(float t) const
{
    return _origin + t * _direction;
}

/**
 * Sets the origin point of the ray.
 *
 * @param origin The new origin point of the ray.
 */
void Ray::setOrigin(const Point3D &origin)
{
    _origin = origin;
}

/**
 * Sets the direction of the ray.
 *
 * @param direction The new direction of the ray.
 */
void Ray::setDirection(const Vec3D &direction)
{
    _direction = direction;
}

/**
 * Returns the origin point of the ray.
 *
 * @return The origin point of the ray.
 */
Point3D Ray::getOrigin(void) const
{
    return _origin;
}

/**
 * @brief Returns the direction of the ray.
 *
 * @return The direction of the ray as a Vec3D object.
 */
Vec3D Ray::getDirection(void) const
{
    return _direction;
}
