/*
** EPITECH PROJECT, 2024
** raytracer
** File description:
** Plane.cpp
*/

#include "Plane.hpp"
#include "Vec3D.hpp"
#include <iostream>

using namespace RayTracer;

Plane::Plane(const Vec3D &normal, const Point3D &point, uint _materialIndex)
    : _normal(normal), _point(point), _materialIndex(_materialIndex)
{
}

Plane::~Plane(void)
{
}

void Plane::doTransformation(const ITransformation &t)
{
    (void)t;
}

bool Plane::hit(const Ray &ray, HitPayload &payload, size_t i) const
{
    float denominator = vec3DDot(getNormal(), ray.getDirection());

    if (fabs(denominator) < Ray::EPSILON)
        return false;

    float distance = vec3DDot(getNormal(), getPoint() - ray.getOrigin()) / denominator;

    if (distance >= 0.0f && distance < payload.getHitDistance())
    {
        payload.setHitDistance(distance);
        payload.setHitPosition(ray.at(distance));
        Vec3D adjustedNormal = denominator > 0 ? -getNormal() : getNormal();
        payload.setHitNormal(adjustedNormal);
        payload.setHitIndex(i);
        return true;
    }
    return false;
}

void Plane::setNormal(const Vec3D &normal)
{
    _normal = normal;
}

void Plane::setPoint(const Point3D &point)
{
    _point = point;
}

void Plane::setMaterialIndex(const uint &index)
{
    _materialIndex = index;
}

Vec3D Plane::getNormal(void) const
{
    return _normal;
}

Point3D Plane::getPoint(void) const
{
    return _point;
}

uint Plane::getMaterialIndex(void) const
{
    return _materialIndex;
}
