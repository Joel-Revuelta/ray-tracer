/*
** EPITECH PROJECT, 2024
** raytracer
** File description:
** Disk
*/

#include "Disk.hpp"
#include <algorithm>
#include <cmath>
#include <stdexcept>

using namespace RayTracer;

namespace
{
Vec3D stableTangent(const Vec3D &normal)
{
    const Vec3D axis = std::fabs(normal.getY()) < 0.999f ? Vec3D(0.0f, 1.0f, 0.0f) : Vec3D(1.0f, 0.0f, 0.0f);
    Vec3D tangent = vec3DCross(axis, normal);
    const float lengthSquared = tangent.lengthSquared();

    if (!std::isfinite(lengthSquared) || lengthSquared < 1e-12f)
        return Vec3D(0.0f);
    return tangent / std::sqrt(lengthSquared);
}
} // namespace

Disk::Disk(const Vec3D &center, const Vec3D &normal, float radius, float innerRadius, uint materialIndex)
    : _center(center), _normal(vec3DNormalize(normal)), _radius(radius), _innerRadius(innerRadius),
      _materialIndex(materialIndex)
{
    if (!std::isfinite(radius) || radius <= 0.0f)
        throw std::invalid_argument("Disk radius must be positive");
    if (!std::isfinite(innerRadius) || innerRadius < 0.0f || innerRadius >= radius)
        throw std::invalid_argument("Disk inner radius must be in [0, radius)");
    if (_normal.lengthSquared() < 1e-12f)
        throw std::invalid_argument("Disk normal must be non-zero");
}

Disk::~Disk(void)
{
}

void Disk::doTransformation(const ITransformation &t)
{
    t.doTransformation(*this);
}

bool Disk::hit(const Ray &ray, HitPayload &payload, size_t i) const
{
    const float denominator = vec3DDot(ray.getDirection(), _normal);

    if (std::fabs(denominator) < 1e-12f)
        return false;

    const float distance = vec3DDot(_center - ray.getOrigin(), _normal) / denominator;

    if (distance <= Ray::EPSILON || distance >= payload.getHitDistance())
        return false;

    const Point3D hitPosition = ray.at(distance);
    const Vec3D relative = hitPosition - _center;
    const float normalProjection = vec3DDot(relative, _normal);
    const float radialSquared = std::max(relative.lengthSquared() - normalProjection * normalProjection, 0.0f);
    const float radiusSquared = _radius * _radius;
    const float innerSquared = _innerRadius * _innerRadius;

    if (radialSquared > radiusSquared || radialSquared < innerSquared)
        return false;

    Vec3D normal = _normal;

    if (vec3DDot(normal, ray.getDirection()) > 0.0f)
        normal = -normal;

    payload.setHitDistance(distance);
    payload.setHitPosition(hitPosition);
    payload.setHitNormal(normal);
    payload.setMaterialIndex(-1);
    payload.setHitIndex(static_cast<int>(i));

    const Vec3D tangent = stableTangent(_normal);
    const Vec3D bitangent = vec3DCross(_normal, tangent);
    const float u = 0.5f + vec3DDot(relative, tangent) / _radius * 0.5f;
    const float v = 0.5f + vec3DDot(relative, bitangent) / _radius * 0.5f;

    payload.setUV(std::clamp(u, 0.0f, 1.0f), std::clamp(v, 0.0f, 1.0f));
    return true;
}

std::shared_ptr<IPrimitive> Disk::clone(void) const
{
    return std::make_shared<Disk>(*this);
}

bool Disk::getData(PrimitiveData &data) const
{
    data.type = PrimitiveType::DISK;
    data.materialIndex = _materialIndex;
    data.center = _center;
    data.normal = _normal;
    data.radius = _radius;
    data.innerRadius = _innerRadius;
    return true;
}

uint Disk::getMaterialIndex(void) const
{
    return _materialIndex;
}

void Disk::setMaterialIndex(const uint &index)
{
    _materialIndex = index;
}

const Vec3D &Disk::getCenter(void) const
{
    return _center;
}

const Vec3D &Disk::getNormal(void) const
{
    return _normal;
}

float Disk::getRadius(void) const
{
    return _radius;
}

float Disk::getInnerRadius(void) const
{
    return _innerRadius;
}

extern "C" std::shared_ptr<IPrimitive> create(Vec3D center, Vec3D normal, double radius, double innerRadius,
                                                uint materialIndex)
{
    return std::make_shared<Disk>(center, normal, static_cast<float>(radius), static_cast<float>(innerRadius),
                                  materialIndex);
}
