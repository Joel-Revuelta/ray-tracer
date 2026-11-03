/*
** EPITECH PROJECT, 2024
** raytracer
** File description:
** MeshPrimitive
*/

#include "MeshPrimitive.hpp"
#include <cmath>
#include <utility>

using namespace RayTracer;

MeshPrimitive::MeshPrimitive(std::shared_ptr<const MeshAsset> asset, uint materialIndex)
    : _asset(std::move(asset)), _materialIndex(materialIndex)
{
}

void MeshPrimitive::doTransformation(const ITransformation &t)
{
    t.doTransformation(*this);
}

bool MeshPrimitive::hitTriangle(const Triangle &triangle, const Ray &ray, HitPayload &payload, size_t i) const
{
    const Vec3D edge1 = triangle.v1 - triangle.v0;
    const Vec3D edge2 = triangle.v2 - triangle.v0;
    const Vec3D pvec = vec3DCross(ray.getDirection(), edge2);
    const float determinant = vec3DDot(edge1, pvec);

    if (!std::isfinite(determinant) || std::fabs(determinant) < 1e-9f)
        return false;

    const float inverseDeterminant = 1.0f / determinant;
    const Vec3D tvec = ray.getOrigin() - triangle.v0;
    const float u = vec3DDot(tvec, pvec) * inverseDeterminant;

    if (u < 0.0f || u > 1.0f)
        return false;

    const Vec3D qvec = vec3DCross(tvec, edge1);
    const float v = vec3DDot(ray.getDirection(), qvec) * inverseDeterminant;

    if (v < 0.0f || u + v > 1.0f)
        return false;

    const float distance = vec3DDot(edge2, qvec) * inverseDeterminant;

    if (distance <= Ray::EPSILON || distance >= payload.getHitDistance())
        return false;

    Vec3D geometricNormal = vec3DCross(edge1, edge2);
    const float w0 = 1.0f - u - v;
    Vec3D shadingNormal = triangle.n0 * w0 + triangle.n1 * u + triangle.n2 * v;
    const float textureU = triangle.tu0 * w0 + triangle.tu1 * u + triangle.tu2 * v;
    const float textureV = triangle.tv0 * w0 + triangle.tv1 * u + triangle.tv2 * v;

    if (!std::isfinite(shadingNormal.lengthSquared()) || shadingNormal.lengthSquared() < 1e-12f)
        shadingNormal = geometricNormal;

    if (vec3DDot(geometricNormal, ray.getDirection()) > 0.0f)
        geometricNormal = -geometricNormal;
    if (vec3DDot(shadingNormal, ray.getDirection()) > 0.0f)
        shadingNormal = -shadingNormal;

    payload.setHitDistance(distance);
    payload.setHitPosition(ray.at(distance));
    payload.setHitNormal(vec3DNormalize(geometricNormal));
    payload.setShadingNormal(vec3DNormalize(shadingNormal));
    payload.setUV(textureU, textureV);
    payload.setHitIndex(static_cast<int>(i));
    return true;
}

bool MeshPrimitive::hitBVH(int nodeIndex, const Ray &ray, HitPayload &payload, size_t i) const
{
    if (nodeIndex < 0 || !_asset)
        return false;

    const BVHNode &node = _asset->bvh.nodes[static_cast<std::size_t>(nodeIndex)];

    if (!node.bounds.intersect(ray, payload.getHitDistance()))
        return false;

    if (node.leaf)
    {
        bool hit = false;

        for (std::size_t index = 0; index < node.count; ++index)
        {
            const std::uint32_t triangleIndex = _asset->bvh.indices[node.start + index];

            if (triangleIndex < _asset->triangles.size() &&
                hitTriangle(_asset->triangles[triangleIndex], ray, payload, i))
                hit = true;
        }
        return hit;
    }

    const BVHNode &leftNode = _asset->bvh.nodes[static_cast<std::size_t>(node.left)];
    const BVHNode &rightNode = _asset->bvh.nodes[static_cast<std::size_t>(node.right)];
    const float leftDistance = leftNode.bounds.entryDistance(ray);
    const float rightDistance = rightNode.bounds.entryDistance(ray);
    const int firstChild = leftDistance <= rightDistance ? node.left : node.right;
    const int secondChild = leftDistance <= rightDistance ? node.right : node.left;
    const float firstDistance = leftDistance <= rightDistance ? leftDistance : rightDistance;
    const float secondDistance = leftDistance <= rightDistance ? rightDistance : leftDistance;

    bool hit = false;

    if (firstDistance < payload.getHitDistance() && hitBVH(firstChild, ray, payload, i))
        hit = true;
    if (secondDistance < payload.getHitDistance() && hitBVH(secondChild, ray, payload, i))
        hit = true;
    return hit;
}

bool MeshPrimitive::hit(const Ray &ray, HitPayload &payload, size_t i) const
{
    if (!_asset || _asset->triangles.empty())
        return false;

    if (!_asset->bvh.nodes.empty())
        return hitBVH(0, ray, payload, i);

    bool hit = false;

    for (const Triangle &triangle : _asset->triangles)
    {
        if (hitTriangle(triangle, ray, payload, i))
            hit = true;
    }
    return hit;
}

std::shared_ptr<IPrimitive> MeshPrimitive::clone(void) const
{
    return std::make_shared<MeshPrimitive>(_asset, _materialIndex);
}

bool MeshPrimitive::getData(PrimitiveData &data) const
{
    if (!_asset)
        return false;

    data.type = PrimitiveType::MESH;
    data.materialIndex = _materialIndex;
    data.localMin = _asset->bounds.min;
    data.localMax = _asset->bounds.max;
    return true;
}

std::shared_ptr<const MeshAsset> MeshPrimitive::getMeshAsset(void) const
{
    return _asset;
}

uint MeshPrimitive::getMaterialIndex(void) const
{
    return _materialIndex;
}

void MeshPrimitive::setMaterialIndex(const uint &index)
{
    _materialIndex = index;
}
