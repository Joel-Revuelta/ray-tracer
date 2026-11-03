/*
** EPITECH PROJECT, 2024
** raytracer
** File description:
** Cone
*/

#include "Cone.hpp"
#include <cmath>

using namespace RayTracer;

namespace
{
constexpr float CONE_EPSILON = 1e-6f;
} // namespace

Cone::Cone(const float radius, const Vec3D &center, const float height, uint materialIndex)
    : _radius(radius), _center(center), _height(height), _materialIndex(materialIndex)
{
    setRotation(false);
}

Cone::~Cone()
{
}

void Cone::doTransformation(const ITransformation &t)
{
    (void)t;
}

bool Cone::getData(PrimitiveData &data) const
{
    data.type = PrimitiveType::CONE;
    data.materialIndex = _materialIndex;
    data.center = _center;
    data.radius = _radius;
    data.height = _height;
    data.coneBaseAbove = _rotation;
    return true;
}

bool Cone::hit(const Ray &ray, HitPayload &payload, size_t i) const
{
    return intersect(ray, payload, i, _rotation);
}

bool Cone::rotation(const Ray &ray, HitPayload &payload, size_t i) const
{
    return intersect(ray, payload, i, true);
}

bool Cone::intersect(const Ray &ray, HitPayload &payload, size_t i, bool baseAbove) const
{
    if (_radius <= 0.0 || _height <= 0.0)
        return false;

    const Vec3D origin = ray.getOrigin();
    const Vec3D direction = ray.getDirection();
    const Vec3D originToCenter = origin - _center;
    const float k = static_cast<float>(_radius / _height);
    const float k2 = k * k;

    float bestDistance = payload.getHitDistance();
    Vec3D bestNormal(0.0f);
    bool found = false;

    const float a = direction.getX() * direction.getX() + direction.getZ() * direction.getZ() -
                    k2 * direction.getY() * direction.getY();
    const float b = 2.0f * (originToCenter.getX() * direction.getX() +
                            originToCenter.getZ() * direction.getZ() -
                            k2 * originToCenter.getY() * direction.getY());
    const float c = originToCenter.getX() * originToCenter.getX() +
                    originToCenter.getZ() * originToCenter.getZ() -
                    k2 * originToCenter.getY() * originToCenter.getY();

    auto tryBody = [&](const float distance) {
        if (distance < 0.0f || distance >= bestDistance)
            return;

        const Vec3D hitPoint = ray.at(distance);
        const float hitY = hitPoint.getY();
        const bool insideHeight = baseAbove
                                      ? (hitY >= _center.getY() && hitY <= _center.getY() + _height)
                                      : (hitY >= _center.getY() - _height && hitY <= _center.getY());
        if (!insideHeight)
            return;

        const Vec3D normal(hitPoint.getX() - _center.getX(), k2 * (_center.getY() - hitY),
                           hitPoint.getZ() - _center.getZ());
        if (normal.lengthSquared() < CONE_EPSILON)
            return;

        bestDistance = distance;
        bestNormal = normal;
        found = true;
    };

    if (std::fabs(a) > CONE_EPSILON)
    {
        const float discriminant = b * b - 4.0f * a * c;

        if (discriminant >= 0.0f)
        {
            const float sqrtD = std::sqrt(discriminant);
            tryBody((-b - sqrtD) / (2.0f * a));
            tryBody((-b + sqrtD) / (2.0f * a));
        }
    }
    else if (std::fabs(b) > CONE_EPSILON)
        tryBody(-c / b);

    // Base cap.
    const float baseY = _center.getY() + static_cast<float>(baseAbove ? _height : -_height);
    const Vec3D baseNormal(0.0f, baseAbove ? 1.0f : -1.0f, 0.0f);

    if (std::fabs(direction.getY()) > CONE_EPSILON)
    {
        const float distance = (baseY - origin.getY()) / direction.getY();

        if (distance >= 0.0f && distance < bestDistance)
        {
            const Vec3D hitPoint = ray.at(distance);
            const float dx = hitPoint.getX() - _center.getX();
            const float dz = hitPoint.getZ() - _center.getZ();

            if (dx * dx + dz * dz <= static_cast<float>(_radius * _radius))
            {
                bestDistance = distance;
                bestNormal = baseNormal;
                found = true;
            }
        }
    }

    if (!found)
        return false;

    if (vec3DDot(direction, bestNormal) > 0.0f)
        bestNormal = -bestNormal;

    payload.setHitDistance(bestDistance);
    payload.setHitPosition(ray.at(bestDistance));
    payload.setHitNormal(vec3DNormalize(bestNormal));
    payload.setHitIndex(i);
    return true;
}

void Cone::setMaterialIndex(const uint &index)
{
    _materialIndex = index;
}

void Cone::setCenter(const Vec3D &center)
{
    _center = center;
}

void Cone::setRadius(const double radius)
{
    _radius = radius;
}

void Cone::setHeight(const double height)
{
    _height = height;
}

uint Cone::getMaterialIndex(void) const
{
    return _materialIndex;
}

Vec3D Cone::getCenter(void) const
{
    return _center;
}

float Cone::getRadius(void) const
{
    return _radius;
}

float Cone::getHeight(void) const
{
    return _height;
}

void Cone::setRotation(bool r)
{
    _rotation = r;
}