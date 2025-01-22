/*
** EPITECH PROJECT, 2024
** raytracer
** File description:
** Sphere
*/

#include <Sphere.hpp>
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
    float distance = 0.0f;
    float a = vec3DDot(ray.getDirection(), ray.getDirection());
    float b = 2.0f * vec3DDot(ray.getDirection(), ray.getOrigin() - getCenter());
    float c = vec3DDot(ray.getOrigin() - getCenter(), ray.getOrigin() - getCenter()) - getRadius() * getRadius();
    float discriminant = b * b - 4.0f * a * c;

    if (discriminant < 0)
        return false;

    distance = (-b - sqrt(discriminant)) / (2.0f * a);

    // If the distance is negative, the ray is pointing in the wrong direction. We want to ignore it.
    if (distance >= 0.0f && distance < payload.getHitDistance())
    {
        // Update the payload with the new hit information
        payload.setHitDistance(distance);
        payload.setHitPosition(ray.at(distance));
        payload.setHitNormal((payload.getHitPosition() - getCenter()) / getRadius());
        payload.setHitIndex(i);
    }
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
