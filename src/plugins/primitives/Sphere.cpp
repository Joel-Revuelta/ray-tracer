/*
** EPITECH PROJECT, 2024
** raytracer
** File description:
** Sphere
*/

#include <Sphere.hpp>
#include <cmath>
#include <iostream>

using namespace RayTracer;

Sphere::Sphere(const float radius, const Vec3D &center, uint _materialIndex)
    : _radius(radius), _center(center), _materialIndex(_materialIndex)
{
}

Sphere::~Sphere(void)
{
}

void Sphere::doTransformation(const ITransformation &t)
{
    (void)t;
}

bool Sphere::hit(const Ray &ray, HitPayload &payload, size_t i) const
{
    const Vec3D direction = ray.getDirection();
    const float a = vec3DDot(direction, direction);

    // A zero-length ray cannot intersect anything, and a non-positive radius
    // describes an empty sphere.
    if (getRadius() <= 0.0f || a < Ray::EPSILON)
        return false;

    const Vec3D originToCenter = ray.getOrigin() - getCenter();
    const float b = 2.0f * vec3DDot(direction, originToCenter);
    const float c = vec3DDot(originToCenter, originToCenter) - getRadius() * getRadius();
    const float discriminant = b * b - 4.0f * a * c;

    if (discriminant < 0.0f)
        return false;

    const float sqrtDiscriminant = std::sqrt(discriminant);
    const float nearRoot = (-b - sqrtDiscriminant) / (2.0f * a);
    const float farRoot = (-b + sqrtDiscriminant) / (2.0f * a);

    // When the ray starts inside the sphere the near root is behind the
    // camera, so the far root is the visible surface. Require a strictly
    // positive distance so a ray starting on the surface cannot hit itself.
    float distance = -1.0f;
    if (nearRoot > Ray::EPSILON)
        distance = nearRoot;
    else if (farRoot > Ray::EPSILON)
        distance = farRoot;
    else
        return false;

    if (distance >= payload.getHitDistance())
        return false;

    payload.setHitDistance(distance);
    payload.setHitPosition(ray.at(distance));

    // Keep the normal on the side the ray came from, matching the other
    // primitives.
    Vec3D normal = (payload.getHitPosition() - getCenter()) / getRadius();
    if (vec3DDot(normal, direction) > 0.0f)
        normal = -normal;

    payload.setHitNormal(vec3DNormalize(normal));
    payload.setHitIndex(i);
    return true;
}

void Sphere::setMaterialIndex(const uint &index)
{
    _materialIndex = index;
}

void Sphere::setRadius(const float radius)
{
    _radius = radius;
}

void Sphere::setCenter(const Point3D center)
{
    _center = center;
}

uint Sphere::getMaterialIndex(void) const
{
    return _materialIndex;
}

float Sphere::getRadius(void) const
{
    return _radius;
}

Point3D Sphere::getCenter(void) const
{
    return _center;
}

// extern "C" std::shared_ptr<IPrimitive> create(const double radius, const Vec3D &center, const uint materialIdx)
// {
//     return std::make_shared<Sphere>(radius, center, materialIdx);
// }
