/*
** EPITECH PROJECT, 2024
** raytracer
** File description:
** Box
*/

#include "Box.hpp"
#include "render/Tangent.hpp"
#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>

using namespace RayTracer;

Box::Box(const Vec3D &minimum, const Vec3D &maximum, uint materialIndex)
    : _minimum(minimum), _maximum(maximum), _materialIndex(materialIndex)
{
    if (!(minimum.getX() < maximum.getX()) || !(minimum.getY() < maximum.getY()) ||
        !(minimum.getZ() < maximum.getZ()))
        throw std::invalid_argument("Box minimum must be strictly below maximum on every axis");
}

Box::~Box(void)
{
}

void Box::doTransformation(const ITransformation &t)
{
    t.doTransformation(*this);
}

bool Box::hit(const Ray &ray, HitPayload &payload, size_t i) const
{
    const Vec3D origin = ray.getOrigin();
    const Vec3D direction = ray.getDirection();
    const float originValues[3] = {origin.getX(), origin.getY(), origin.getZ()};
    const float directionValues[3] = {direction.getX(), direction.getY(), direction.getZ()};
    const float minimumValues[3] = {_minimum.getX(), _minimum.getY(), _minimum.getZ()};
    const float maximumValues[3] = {_maximum.getX(), _maximum.getY(), _maximum.getZ()};
    float nearDistance = -std::numeric_limits<float>::infinity();
    float farDistance = std::numeric_limits<float>::infinity();
    int nearAxis = -1;
    float nearSign = 1.0f;

    for (int axis = 0; axis < 3; ++axis)
    {
        if (std::fabs(directionValues[axis]) < 1e-12f)
        {
            if (originValues[axis] < minimumValues[axis] || originValues[axis] > maximumValues[axis])
                return false;
            continue;
        }

        const float inverse = 1.0f / directionValues[axis];
        float first = (minimumValues[axis] - originValues[axis]) * inverse;
        float second = (maximumValues[axis] - originValues[axis]) * inverse;
        float sign = -1.0f;

        if (first > second)
        {
            std::swap(first, second);
            sign = 1.0f;
        }
        if (first > nearDistance)
        {
            nearDistance = first;
            nearAxis = axis;
            nearSign = sign;
        }
        farDistance = std::min(farDistance, second);

        if (nearDistance > farDistance)
            return false;
    }

    if (nearAxis < 0)
        return false;

    float distance = nearDistance;

    if (distance <= Ray::EPSILON)
    {
        distance = farDistance;
        nearSign = -nearSign;
    }
    if (distance <= Ray::EPSILON || distance >= payload.getHitDistance())
        return false;

    const Point3D hitPosition = ray.at(distance);
    Vec3D normal(0.0f);

    if (nearAxis == 0)
        normal.setX(nearSign);
    else if (nearAxis == 1)
        normal.setY(nearSign);
    else
        normal.setZ(nearSign);

    if (vec3DDot(normal, direction) > 0.0f)
        normal = -normal;

    payload.setHitDistance(distance);
    payload.setHitPosition(hitPosition);
    payload.setHitNormal(vec3DNormalize(normal));
    payload.setMaterialIndex(-1);
    payload.setHitIndex(static_cast<int>(i));

    const Vec3D size = _maximum - _minimum;
    const Vec3D local = hitPosition - _minimum;
    float u = 0.0f;
    float v = 0.0f;

    if (nearAxis == 0)
    {
        u = local.getY() / std::max(size.getY(), 1e-12f);
        v = local.getZ() / std::max(size.getZ(), 1e-12f);
    }
    else if (nearAxis == 1)
    {
        u = local.getX() / std::max(size.getX(), 1e-12f);
        v = local.getZ() / std::max(size.getZ(), 1e-12f);
    }
    else
    {
        u = local.getX() / std::max(size.getX(), 1e-12f);
        v = local.getY() / std::max(size.getY(), 1e-12f);
    }

    payload.setUV(std::clamp(u, 0.0f, 1.0f), std::clamp(v, 0.0f, 1.0f));

    const Vec3D tangent = nearAxis == 0 ? Vec3D(0.0f, 1.0f, 0.0f) : Vec3D(1.0f, 0.0f, 0.0f);

    payload.setShadingTangent(orthonormalizeTangent(tangent, payload.getShadingNormal()));
    return true;
}

std::shared_ptr<IPrimitive> Box::clone(void) const
{
    return std::make_shared<Box>(*this);
}

bool Box::getData(PrimitiveData &data) const
{
    data.type = PrimitiveType::BOX;
    data.materialIndex = _materialIndex;
    data.localMin = _minimum;
    data.localMax = _maximum;
    return true;
}

uint Box::getMaterialIndex(void) const
{
    return _materialIndex;
}

void Box::setMaterialIndex(const uint &index)
{
    _materialIndex = index;
}

const Vec3D &Box::getMinimum(void) const
{
    return _minimum;
}

const Vec3D &Box::getMaximum(void) const
{
    return _maximum;
}

extern "C" std::shared_ptr<IPrimitive> create(Vec3D minimum, Vec3D maximum, uint materialIndex)
{
    return std::make_shared<Box>(minimum, maximum, materialIndex);
}
