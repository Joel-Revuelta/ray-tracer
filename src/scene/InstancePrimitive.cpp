/*
** EPITECH PROJECT, 2024
** raytracer
** File description:
** InstancePrimitive
*/

#include "InstancePrimitive.hpp"
#include <cmath>
#include <utility>

using namespace RayTracer;

InstancePrimitive::InstancePrimitive(std::shared_ptr<IPrimitive> child, const TransformData &transform)
    : _child(std::move(child)), _transform(transform)
{
}

InstancePrimitive::InstancePrimitive(std::shared_ptr<IPrimitive> child, const TransformPose &pose)
    : _child(std::move(child)), _transform(makeTransform(pose)), _pose(pose), _hasPose(true)
{
}

void InstancePrimitive::doTransformation(const ITransformation &t)
{
    // The legacy transformation plugins mutate primitive geometry directly.
    // Keep that behaviour for the wrapped child; the instance transform is
    // applied on top of it.
    if (_child)
        _child->doTransformation(t);
}

bool InstancePrimitive::hit(const Ray &ray, HitPayload &payload, size_t i) const
{
    if (!_child)
        return false;

    const Vec3D localDirection = transformInverseVector(_transform, ray.getDirection());
    const float directionLength = localDirection.length();

    if (!std::isfinite(directionLength) || directionLength <= 0.0f)
        return false;

    const Ray localRay(transformInversePoint(_transform, ray.getOrigin()), localDirection / directionLength);
    HitPayload localPayload = payload;

    // A material override belongs to the primitive that recorded the hit. Do
    // not let a previous mesh hit leak through an instance-wrapped non-mesh.
    localPayload.setMaterialIndex(-1);

    // Distances scale linearly with the ray direction, so convert the current
    // best distance into the normalized local space before testing.
    localPayload.setHitDistance(payload.getHitDistance() * directionLength);

    if (!_child->hit(localRay, localPayload, i))
        return false;

    const float worldDistance = localPayload.getHitDistance() / directionLength;

    if (!std::isfinite(worldDistance) || worldDistance >= payload.getHitDistance())
        return false;

    Vec3D normal = transformNormal(_transform, localPayload.getHitNormal());
    Vec3D shadingNormal = transformNormal(_transform, localPayload.getShadingNormal());

    if (!std::isfinite(shadingNormal.lengthSquared()) || shadingNormal.lengthSquared() < 1e-12f)
        shadingNormal = normal;

    if (vec3DDot(normal, ray.getDirection()) > 0.0f)
        normal = -normal;
    if (vec3DDot(shadingNormal, ray.getDirection()) > 0.0f)
        shadingNormal = -shadingNormal;

    payload.setHitDistance(worldDistance);
    payload.setHitPosition(ray.at(worldDistance));
    payload.setHitNormal(vec3DNormalize(normal));
    payload.setShadingNormal(vec3DNormalize(shadingNormal));
    payload.setUV(localPayload.getU(), localPayload.getV());
    payload.setMaterialIndex(localPayload.getMaterialIndex());
    payload.setHitIndex(static_cast<int>(i));
    return true;
}

std::shared_ptr<IPrimitive> InstancePrimitive::clone(void) const
{
    std::shared_ptr<IPrimitive> child = _child ? _child->clone() : nullptr;

    if (!child)
        child = _child;

    std::shared_ptr<InstancePrimitive> copy =
        _hasPose ? std::make_shared<InstancePrimitive>(child, _pose)
                 : std::make_shared<InstancePrimitive>(child, _transform);

    if (!_groupId.empty())
        copy->setGroupId(_groupId);
    return copy;
}

bool InstancePrimitive::getData(PrimitiveData &data) const
{
    if (!_child || !_child->getData(data))
        return false;

    data.transform = composeTransforms(_transform, data.transform);
    return true;
}

std::shared_ptr<const MeshAsset> InstancePrimitive::getMeshAsset(void) const
{
    return _child ? _child->getMeshAsset() : nullptr;
}

std::shared_ptr<const std::vector<int>> InstancePrimitive::getMeshMaterialMap(void) const
{
    return _child ? _child->getMeshMaterialMap() : nullptr;
}

void InstancePrimitive::shiftMaterialIndices(uint offset)
{
    if (_child)
        _child->shiftMaterialIndices(offset);
}

uint InstancePrimitive::getMaterialIndex(void) const
{
    return _child ? _child->getMaterialIndex() : 0;
}

void InstancePrimitive::setMaterialIndex(const uint &index)
{
    if (_child)
        _child->setMaterialIndex(index);
}

const TransformData &InstancePrimitive::getTransform(void) const
{
    return _transform;
}

const TransformPose &InstancePrimitive::getPose(void) const
{
    return _pose;
}

bool InstancePrimitive::hasPose(void) const
{
    return _hasPose;
}

void InstancePrimitive::setTransform(const TransformData &transform)
{
    _transform = transform;
}

void InstancePrimitive::setPose(const TransformPose &pose)
{
    _pose = pose;
    _transform = makeTransform(pose);
    _hasPose = true;
}

void InstancePrimitive::setGroupId(const std::string &groupId)
{
    _groupId = groupId;
}

const std::string &InstancePrimitive::getGroupId(void) const
{
    return _groupId;
}
