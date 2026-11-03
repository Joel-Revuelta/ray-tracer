/*
** EPITECH PROJECT, 2023
** raytracer
** File description:
** HitPayload.cpp
*/

#include "HitPayload.hpp"

using namespace RayTracer;

HitPayload::HitPayload(void)
    : _objectIndex(-1), _materialIndex(-1), _hitNormal(Vec3D(0, 0, 0)), _shadingNormal(Vec3D(0, 0, 0)),
      _hitDistance(std::numeric_limits<float>::infinity()), _hitPosition(Point3D(0, 0, 0))
{
}

HitPayload::~HitPayload(void)
{
}

void HitPayload::setHitIndex(const int &objectIndex)
{
    _objectIndex = objectIndex;
}

void HitPayload::setMaterialIndex(const int &materialIndex)
{
    _materialIndex = materialIndex;
}

void HitPayload::setHitNormal(const Vec3D &hitNormal)
{
    _hitNormal = hitNormal;
    _shadingNormal = hitNormal;
}

void HitPayload::setShadingNormal(const Vec3D &shadingNormal)
{
    _shadingNormal = shadingNormal;
}

void HitPayload::setHitDistance(const float &hitDistance)
{
    _hitDistance = hitDistance;
}

void HitPayload::setHitPosition(const Point3D &hitPosition)
{
    _hitPosition = hitPosition;
}

void HitPayload::setUV(const float &u, const float &v)
{
    _u = u;
    _v = v;
}

int HitPayload::getHitIndex(void) const
{
    return _objectIndex;
}

int HitPayload::getMaterialIndex(void) const
{
    return _materialIndex;
}

Vec3D HitPayload::getHitNormal(void) const
{
    return _hitNormal;
}

Vec3D HitPayload::getShadingNormal(void) const
{
    return _shadingNormal;
}

float HitPayload::getHitDistance(void) const
{
    return _hitDistance;
}

Point3D HitPayload::getHitPosition(void) const
{
    return _hitPosition;
}

float HitPayload::getU(void) const
{
    return _u;
}

float HitPayload::getV(void) const
{
    return _v;
}
