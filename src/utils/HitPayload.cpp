/*
** EPITECH PROJECT, 2023
** raytracer
** File description:
** HitPayload.cpp
*/

#include "HitPayload.hpp"

using namespace RayTracer;

HitPayload::HitPayload(void)
    : _objectIndex(-1), _hitNormal(Vec3D(0, 0, 0)), _hitDistance(std::numeric_limits<float>::infinity()),
      _hitPosition(Point3D(0, 0, 0))
{
}

HitPayload::~HitPayload(void)
{
}

void HitPayload::setHitIndex(const int &objectIndex)
{
    _objectIndex = objectIndex;
}

void HitPayload::setHitNormal(const Vec3D &hitNormal)
{
    _hitNormal = hitNormal;
}

void HitPayload::setHitDistance(const float &hitDistance)
{
    _hitDistance = hitDistance;
}

void HitPayload::setHitPosition(const Point3D &hitPosition)
{
    _hitPosition = hitPosition;
}

int HitPayload::getHitIndex(void) const
{
    return _objectIndex;
}

Vec3D HitPayload::getHitNormal(void) const
{
    return _hitNormal;
}

float HitPayload::getHitDistance(void) const
{
    return _hitDistance;
}

Point3D HitPayload::getHitPosition(void) const
{
    return _hitPosition;
}
