/*
** EPITECH PROJECT, 2024
** raytracer
** File description:
** Cone
*/

#include "Cone.hpp"
#include <iostream>
using namespace RayTracer;

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

// !GOOD ONE
bool Cone::hit(const Ray &ray, HitPayload &payload, size_t i) const
{
    if (_rotation)
        return rotation(ray, payload, i);

    Vec3D oc = ray.getOrigin() - _center;

    float k = _radius / _height;
    float a = ray.getDirection().getX() * ray.getDirection().getX() +
              ray.getDirection().getZ() * ray.getDirection().getZ() -
              k * k * ray.getDirection().getY() * ray.getDirection().getY();
    float b = 2 * (oc.getX() * ray.getDirection().getX() + oc.getZ() * ray.getDirection().getZ() -
                   k * k * oc.getY() * ray.getDirection().getY());
    float c = oc.getX() * oc.getX() + oc.getZ() * oc.getZ() - k * k * oc.getY() * oc.getY();

    float discriminant = b * b - 4 * a * c;

    if (discriminant < 0)
        return false;

    float sqrtD = sqrt(discriminant);
    float distance = (-b - sqrtD) / (2 * a);

    if (distance < 0.0f || distance >= payload.getHitDistance())
        return false;

    Vec3D hitPoint = ray.at(distance);

    // Check if hit point is within cone's height
    float hitY = hitPoint.getY();
    if (hitY > _center.getY() || hitY < _center.getY() - _height)
        return false;

    // Vec3D normal_vect = Vec3D((hitPoint.getX() - _center.getX()) / _radius, -1 * (_center.getY() - hitY) / _height,
    // (hitPoint.getZ() - _center.getZ()) / _radius);
    Vec3D normal_vect = Vec3D((hitPoint.getX() - _center.getX()) / _radius, k * k * (_center.getY() - hitY) / _height,
                              (hitPoint.getZ() - _center.getZ()) / _radius);

    if (vec3DDot(ray.getDirection(), normal_vect) > 0)
        normal_vect = -normal_vect;

    payload.setHitDistance(distance);
    payload.setHitPosition(hitPoint);
    payload.setHitNormal(normal_vect);
    payload.setHitIndex(i);

    return true;
}

bool Cone::rotation(const Ray &ray, HitPayload &payload, size_t i) const
{
    Vec3D oc = ray.getOrigin() - _center;

    float k = _radius / _height;
    float a = ray.getDirection().getX() * ray.getDirection().getX() + ray.getDirection().getZ() *
    ray.getDirection().getZ() - k * k * ray.getDirection().getY() * ray.getDirection().getY(); float b = 2 *
    (oc.getX() * ray.getDirection().getX() + oc.getZ() * ray.getDirection().getZ() - k * k * oc.getY() *
    ray.getDirection().getY()); float c = oc.getX() * oc.getX() + oc.getZ() * oc.getZ() - k * k * oc.getY() *
    oc.getY();

    float discriminant = b * b - 4 * a * c;

    if (discriminant < 0)
        return false;

    float sqrtD = sqrt(discriminant);
    float distance = (-b - sqrtD) / (2 * a);

    if (distance < 0.0f || distance >= payload.getHitDistance())
        return false;

    Vec3D hitPoint = ray.at(distance);

    float hitY = hitPoint.getY();
    if (hitY < _center.getY() || hitY > _center.getY() + _height)
        return false;

    Vec3D normal_vect = Vec3D((hitPoint.getX() - _center.getX()) / _radius, k * k * (_center.getY() - hitY) /
    _height, (hitPoint.getZ() - _center.getZ()) / _radius);

    if (vec3DDot(ray.getDirection(), normal_vect) > 0)
        normal_vect = -normal_vect;

    payload.setHitDistance(distance);
    payload.setHitPosition(hitPoint);
    payload.setHitNormal(normal_vect);
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