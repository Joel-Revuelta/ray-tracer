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
    : _normal(vec3DNormalize(normal)), _point(point), _materialIndex(_materialIndex)
{
}

Plane::~Plane(void)
{
}

void Plane::doTransformation(const ITransformation &t)
{
    t.doTransformation(*this);
}

bool Plane::getData(PrimitiveData &data) const
{
    data.type = PrimitiveType::PLANE;
    data.materialIndex = _materialIndex;
    data.point = _point;
    data.normal = _normal;
    return true;
}

std::shared_ptr<IPrimitive> Plane::clone(void) const
{
    return std::make_shared<Plane>(*this);
}

bool Plane::hit(const Ray &ray, HitPayload &payload, size_t i) const
{
    float denominator = vec3DDot(getNormal(), ray.getDirection());

    if (fabs(denominator) < Ray::EPSILON)
        return false;

    float distance = vec3DDot(getNormal(), getPoint() - ray.getOrigin()) / denominator;

    if (distance >= 0.0f && distance < payload.getHitDistance())
    {
        const Vec3D hitPosition = ray.at(distance);

        payload.setHitDistance(distance);
        payload.setHitPosition(hitPosition);
        Vec3D adjustedNormal = denominator > 0 ? -getNormal() : getNormal();
        payload.setHitNormal(adjustedNormal);
        payload.setUV(hitPosition.getX(), hitPosition.getZ());
        payload.setMaterialIndex(-1);
        payload.setHitIndex(i);
        return true;
    }
    return false;
}

void Plane::setNormal(const Vec3D &normal)
{
    _normal = vec3DNormalize(normal);
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
