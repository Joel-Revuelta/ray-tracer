/*
** EPITECH PROJECT, 2024
** raytracer
** File description:
** Cylinder.cpp
*/

#include "Cylinder.hpp"
#include <cmath>

using namespace RayTracer;

namespace
{
constexpr float AXIS_EPSILON = 1e-6f;
} // namespace

Cylinder::Cylinder(const float radius, const Vec3D &center, const float height, const uint materialIndex)
    : _radius(radius), _center(center), _height(height), _materialIndex(materialIndex)
{
    setAxis(Vec3D(0, 90, 0));
}

Cylinder::~Cylinder()
{
}

void Cylinder::doTransformation(const ITransformation &t)
{
    (void)t;
}

//! INFINIT
// bool Cylinder::hit(const Ray &ray, HitPayload &payload, size_t i) const
// {
//     Vec3D oc = ray.getOrigin() - _center;
//     Vec3D dir = ray.getDirection();

//     Vec3D dp = oc - _axis * vec3DDot(oc, _axis);
//     Vec3D dl = dir - _axis * vec3DDot(dir, _axis);

//     double a = vec3DDot(dl, dl);
//     double b = 2 * vec3DDot(dp, dl);  // Corrected here
//     double c = vec3DDot(dp, dp) - _radius * _radius;

//     double discriminant = b * b - 4 * a * c;
//     if (discriminant < 0)
//     {
//         return false;
//     }

//     double sqrtD = sqrt(discriminant);
//     double distance = (-b - sqrtD) / (2 * a);

//     if (distance < 0.0f || distance >= payload.getHitDistance())
//         return false;

//     Vec3D hitPoint = ray.at(distance);
//     Vec3D normal_vect = (hitPoint - _center - _axis * vec3DNormalize(vec3DDot(hitPoint, _axis)));

//     if (vec3DDot(dir, normal_vect) > 0)
//         normal_vect = -normal_vect;

//     payload.setHitDistance(distance);
//     payload.setHitPosition(hitPoint);
//     payload.setHitNormal(normal_vect);
//     payload.setHitIndex(i);

//     return true;
// }

bool Cylinder::hit(const Ray &ray, HitPayload &payload, size_t i) const
{
    const Vec3D axis = vec3DNormalize(_axis);
    const Vec3D origin = ray.getOrigin();
    const Vec3D direction = ray.getDirection();
    const Vec3D originToCenter = origin - _center;

    float bestDistance = payload.getHitDistance();
    Vec3D bestNormal(0.0f);
    bool found = false;

    // Infinite cylinder body, restricted to the segment between both caps.
    const Vec3D originPerp = originToCenter - axis * vec3DDot(originToCenter, axis);
    const Vec3D directionPerp = direction - axis * vec3DDot(direction, axis);
    const float a = vec3DDot(directionPerp, directionPerp);
    const float b = 2.0f * vec3DDot(originPerp, directionPerp);
    const float c = vec3DDot(originPerp, originPerp) - static_cast<float>(_radius * _radius);

    if (std::fabs(a) > AXIS_EPSILON)
    {
        const float discriminant = b * b - 4.0f * a * c;

        if (discriminant >= 0.0f)
        {
            const float sqrtD = std::sqrt(discriminant);
            const float roots[2] = {(-b - sqrtD) / (2.0f * a), (-b + sqrtD) / (2.0f * a)};

            for (const float distance : roots)
            {
                if (distance < 0.0f || distance >= bestDistance)
                    continue;

                const Vec3D hitPoint = ray.at(distance);
                const float heightAtHit = vec3DDot(hitPoint - _center, axis);
                if (heightAtHit < 0.0f || heightAtHit > _height)
                    continue;

                const Vec3D radial = (hitPoint - _center) - axis * heightAtHit;
                if (radial.lengthSquared() < AXIS_EPSILON)
                    continue;

                bestDistance = distance;
                bestNormal = radial;
                found = true;
            }
        }
    }

    // Both end caps. The cylinder starts at _center and extends along _axis.
    const Vec3D capCenters[2] = {_center, _center + axis * static_cast<float>(_height)};
    const Vec3D capNormals[2] = {-axis, axis};

    for (int cap = 0; cap < 2; ++cap)
    {
        const float denominator = vec3DDot(capNormals[cap], direction);
        if (std::fabs(denominator) < AXIS_EPSILON)
            continue;

        const float distance = vec3DDot(capNormals[cap], capCenters[cap] - origin) / denominator;
        if (distance < 0.0f || distance >= bestDistance)
            continue;

        const Vec3D hitPoint = ray.at(distance);
        if ((hitPoint - capCenters[cap]).lengthSquared() > static_cast<float>(_radius * _radius))
            continue;

        bestDistance = distance;
        bestNormal = capNormals[cap];
        found = true;
    }

    if (!found)
        return false;

    // The visible side of the surface always faces the incoming ray.
    if (vec3DDot(direction, bestNormal) > 0.0f)
        bestNormal = -bestNormal;

    payload.setHitDistance(bestDistance);
    payload.setHitPosition(ray.at(bestDistance));
    payload.setHitNormal(vec3DNormalize(bestNormal));
    payload.setHitIndex(i);
    return true;
}

void Cylinder::setCenter(const Vec3D &center)
{
    _center = center;
}

void Cylinder::setRadius(const double radius)
{
    _radius = radius;
}

void Cylinder::setHeight(const double height)
{
    _height = height;
}

Vec3D Cylinder::getCenter(void) const
{
    return _center;
}

float Cylinder::getRadius(void) const
{
    return _radius;
}

float Cylinder::getHeight(void) const
{
    return _height;
}

uint Cylinder::getMaterialIndex(void) const
{
    return _materialIndex;
}

void Cylinder::setMaterialIndex(const uint &index)
{
    _materialIndex = index;
}

void Cylinder::setAxis(const Vec3D &axis)
{
    const Vec3D snapped(std::round(std::sin(axis.getX())), std::round(std::sin(axis.getY())),
                        std::round(std::sin(axis.getZ())));

    if (snapped.lengthSquared() < AXIS_EPSILON)
        _axis = Vec3D(0.0f, 1.0f, 0.0f);
    else
        _axis = vec3DNormalize(snapped);
}

Vec3D Cylinder::getAxis(void) const
{
    return _axis;
}

// extern "C" std::shared_ptr<Primitive> create(const double radius, const Vec3D &center, const double height, const
// uint material)
// {
//     return std::make_shared<Cylinder>(radius, center, height, material);
// }
