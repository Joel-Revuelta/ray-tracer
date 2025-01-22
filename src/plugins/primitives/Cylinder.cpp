/*
** EPITECH PROJECT, 2024
** raytracer
** File description:
** Cylinder.cpp
*/

#include "Cylinder.hpp"

using namespace RayTracer;

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

//! HEIGHT LIMITED
bool Cylinder::hit(const Ray &ray, HitPayload &payload, size_t i) const
{
    Vec3D oc = ray.getOrigin() - _center;
    Vec3D dir = ray.getDirection();

    Vec3D dp = oc - _axis * vec3DDot(oc, _axis);
    Vec3D dl = dir - _axis * vec3DDot(dir, _axis);

    double a = vec3DDot(dl, dl);
    double b = 2 * vec3DDot(dp, dl);
    double c = vec3DDot(dp, dp) - _radius * _radius;

    double discriminant = b * b - 4 * a * c;
    if (discriminant < 0)
    {
        return false;
    }

    double sqrtD = sqrt(discriminant);
    double distance = (-b - sqrtD) / (2 * a);

    if (distance < 0.0f || distance >= payload.getHitDistance())
        return false;

    Vec3D hitPoint = ray.at(distance);
    Vec3D normal_vect = (hitPoint - _center - _axis * vec3DNormalize(vec3DDot(hitPoint, _axis)));

    double heightAtHitPoint = vec3DDot(hitPoint - _center, _axis);
    if (heightAtHitPoint < 0 || heightAtHitPoint > _height)
    {
        return false;
    }

    if (vec3DDot(dir, normal_vect) > 0)
        normal_vect = -normal_vect;

    payload.setHitDistance(distance);
    payload.setHitPosition(hitPoint);
    payload.setHitNormal(normal_vect);
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
    float x = axis.getX();
    float y = axis.getY();
    float z = axis.getZ();

    x = sinf(x);
    y = sinf(y);
    z = sinf(z);

    x = round(x);
    y = round(y);
    z = round(z);

    _axis = Vec3D(x, y, z);
    vec3DNormalize(_axis);
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
