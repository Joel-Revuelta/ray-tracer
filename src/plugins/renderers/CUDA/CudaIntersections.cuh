/*
** EPITECH PROJECT, 2024
** raytracer
** File description:
** CudaIntersections.cuh
*/

#ifndef CUDA_INTERSECTIONS_CUH_
#define CUDA_INTERSECTIONS_CUH_

#include "CudaCommon.cuh"

namespace RayTracer
{
namespace Cuda
{
namespace
{

__device__ bool intersectBounds(const BVHNode &node, const Ray &ray, float maxDistance);
__device__ void pushNodeChildren(const BVHNode *nodes, const BVHNode &node, const Ray &ray, float maxDistance,
                                 int *stack, int &stackSize);

__device__ bool intersectSphere(const Primitive &primitive, const Ray &ray, float &bestDistance, Vec3 &bestNormal,
                                Vec3 &bestShadingNormal, Vec3 &bestTangent, float &bestU, float &bestV, int &bestIndex, int index)
{
    if (primitive.radius <= 0.0f)
        return false;

    const Vec3 originToCenter = sub(ray.origin, primitive.center);
    const float a = dot(ray.direction, ray.direction);

    if (a < DIRECTION_EPSILON)
        return false;

    const float b = 2.0f * dot(ray.direction, originToCenter);
    const float c = dot(originToCenter, originToCenter) - primitive.radius * primitive.radius;
    const float discriminant = b * b - 4.0f * a * c;

    if (discriminant < 0.0f)
        return false;

    const float sqrtDiscriminant = sqrtf(discriminant);
    const float nearRoot = (-b - sqrtDiscriminant) / (2.0f * a);
    const float farRoot = (-b + sqrtDiscriminant) / (2.0f * a);
    float distance = -1.0f;

    if (nearRoot > RAY_EPSILON)
        distance = nearRoot;
    else if (farRoot > RAY_EPSILON)
        distance = farRoot;
    else
        return false;

    if (distance >= bestDistance)
        return false;

    bestDistance = distance;
    bestIndex = index;

    const Vec3 hitPosition = rayAt(ray, distance);
    Vec3 normal = scale(sub(hitPosition, primitive.center), 1.0f / primitive.radius);

    if (dot(normal, ray.direction) > 0.0f)
        normal = negate(normal);
    bestNormal = normalize(normal);
    bestShadingNormal = bestNormal;

    const Vec3 local = scale(sub(hitPosition, primitive.center), 1.0f / primitive.radius);
    const float longitude = atan2f(local.z, local.x);

    bestTangent = makeVec3(-sinf(longitude), 0.0f, cosf(longitude));
    if (lengthSquared(bestTangent) < 1e-12f)
        bestTangent = stableTangent(bestNormal);

    bestU = 0.5f + longitude / (2.0f * PI_F);
    bestV = acosf(fmaxf(-1.0f, fminf(1.0f, local.y))) / PI_F;
    return true;
}

__device__ bool intersectPlane(const Primitive &primitive, const Ray &ray, float &bestDistance, Vec3 &bestNormal,
                               Vec3 &bestShadingNormal, Vec3 &bestTangent, float &bestU, float &bestV, int &bestIndex, int index)
{
    const float denominator = dot(primitive.normal, ray.direction);

    if (fabsf(denominator) < RAY_EPSILON)
        return false;

    const float distance = dot(primitive.normal, sub(primitive.point, ray.origin)) / denominator;

    if (distance <= RAY_EPSILON || distance >= bestDistance)
        return false;

    bestDistance = distance;
    bestIndex = index;
    bestNormal = denominator > 0.0f ? negate(primitive.normal) : primitive.normal;
    bestShadingNormal = bestNormal;
    bestTangent = orthonormalizeTangent(makeVec3(1.0f, 0.0f, 0.0f), bestNormal);

    const Vec3 hitPosition = rayAt(ray, distance);

    bestU = hitPosition.x;
    bestV = hitPosition.z;
    return true;
}

__device__ bool intersectCylinder(const Primitive &primitive, const Ray &ray, float &bestDistance, Vec3 &bestNormal,
                                  Vec3 &bestShadingNormal, Vec3 &bestTangent, float &bestU, float &bestV, int &bestIndex, int index)
{
    if (primitive.radius <= 0.0f || primitive.height <= 0.0f)
        return false;

    const Vec3 axis = normalize(primitive.axis);
    const Vec3 originToCenter = sub(ray.origin, primitive.center);
    const Vec3 originPerp = sub(originToCenter, scale(axis, dot(originToCenter, axis)));
    const Vec3 directionPerp = sub(ray.direction, scale(axis, dot(ray.direction, axis)));
    const float a = dot(directionPerp, directionPerp);
    const float b = 2.0f * dot(originPerp, directionPerp);
    const float c = dot(originPerp, originPerp) - primitive.radius * primitive.radius;
    bool found = false;

    if (fabsf(a) > AXIS_EPSILON)
    {
        const float discriminant = b * b - 4.0f * a * c;

        if (discriminant >= 0.0f)
        {
            const float sqrtDiscriminant = sqrtf(discriminant);
            const float roots[2] = {(-b - sqrtDiscriminant) / (2.0f * a), (-b + sqrtDiscriminant) / (2.0f * a)};

            for (int i = 0; i < 2; ++i)
            {
                const float distance = roots[i];

                if (distance <= RAY_EPSILON || distance >= bestDistance)
                    continue;

                const Vec3 hitPosition = rayAt(ray, distance);
                const float heightAtHit = dot(sub(hitPosition, primitive.center), axis);

                if (heightAtHit < 0.0f || heightAtHit > primitive.height)
                    continue;

                const Vec3 radial = sub(sub(hitPosition, primitive.center), scale(axis, heightAtHit));

                if (lengthSquared(radial) < AXIS_EPSILON)
                    continue;

                bestDistance = distance;
                bestNormal = radial;
                bestIndex = index;
                found = true;
            }
        }
    }

    const Vec3 capCenters[2] = {primitive.center, add(primitive.center, scale(axis, primitive.height))};
    const Vec3 capNormals[2] = {negate(axis), axis};

    for (int cap = 0; cap < 2; ++cap)
    {
        const float denominator = dot(capNormals[cap], ray.direction);

        if (fabsf(denominator) < AXIS_EPSILON)
            continue;

        const float distance = dot(capNormals[cap], sub(capCenters[cap], ray.origin)) / denominator;

        if (distance <= RAY_EPSILON || distance >= bestDistance)
            continue;

        const Vec3 hitPosition = rayAt(ray, distance);

        if (lengthSquared(sub(hitPosition, capCenters[cap])) > primitive.radius * primitive.radius)
            continue;

        bestDistance = distance;
        bestNormal = capNormals[cap];
        bestIndex = index;
        found = true;
    }

    if (!found)
        return false;

    if (dot(ray.direction, bestNormal) > 0.0f)
        bestNormal = negate(bestNormal);
    bestNormal = normalize(bestNormal);
    bestShadingNormal = bestNormal;

    const Vec3 hitPosition = rayAt(ray, bestDistance);
    const Vec3 relative = sub(hitPosition, primitive.center);
    const float heightAtHit = dot(relative, axis);
    const Vec3 radial = sub(relative, scale(axis, heightAtHit));

    const Vec3 circumference = cross(radial, axis);

    bestTangent = lengthSquared(circumference) > 1e-12f ? normalize(circumference) : stableTangent(bestNormal);

    bestU = 0.5f + atan2f(radial.z, radial.x) / (2.0f * PI_F);
    bestV = fmaxf(0.0f, fminf(1.0f, heightAtHit / primitive.height));
    return true;
}

__device__ void tryConeBody(const Primitive &primitive, const Ray &ray, float distance, float k2, float &bestDistance,
                            Vec3 &bestNormal, int &bestIndex, int index, bool &found)
{
    if (distance <= RAY_EPSILON || distance >= bestDistance)
        return;

    const Vec3 hitPosition = rayAt(ray, distance);
    const float hitY = hitPosition.y;
    const bool insideHeight =
        primitive.coneBaseAbove ? (hitY >= primitive.center.y && hitY <= primitive.center.y + primitive.height)
                                : (hitY >= primitive.center.y - primitive.height && hitY <= primitive.center.y);

    if (!insideHeight)
        return;

    const Vec3 normal = makeVec3(hitPosition.x - primitive.center.x, k2 * (primitive.center.y - hitY),
                                 hitPosition.z - primitive.center.z);

    if (lengthSquared(normal) < CONE_EPSILON)
        return;

    bestDistance = distance;
    bestNormal = normal;
    bestIndex = index;
    found = true;
}

__device__ bool intersectCone(const Primitive &primitive, const Ray &ray, float &bestDistance, Vec3 &bestNormal,
                              Vec3 &bestShadingNormal, Vec3 &bestTangent, float &bestU, float &bestV, int &bestIndex, int index)
{
    if (primitive.radius <= 0.0f || primitive.height <= 0.0f)
        return false;

    const Vec3 originToCenter = sub(ray.origin, primitive.center);
    const float k = primitive.radius / primitive.height;
    const float k2 = k * k;
    const float a = ray.direction.x * ray.direction.x + ray.direction.z * ray.direction.z -
                    k2 * ray.direction.y * ray.direction.y;
    const float b = 2.0f * (originToCenter.x * ray.direction.x + originToCenter.z * ray.direction.z -
                            k2 * originToCenter.y * ray.direction.y);
    const float c = originToCenter.x * originToCenter.x + originToCenter.z * originToCenter.z -
                    k2 * originToCenter.y * originToCenter.y;
    bool found = false;

    if (fabsf(a) > CONE_EPSILON)
    {
        const float discriminant = b * b - 4.0f * a * c;

        if (discriminant >= 0.0f)
        {
            const float sqrtDiscriminant = sqrtf(discriminant);
            tryConeBody(primitive, ray, (-b - sqrtDiscriminant) / (2.0f * a), k2, bestDistance, bestNormal, bestIndex,
                        index, found);
            tryConeBody(primitive, ray, (-b + sqrtDiscriminant) / (2.0f * a), k2, bestDistance, bestNormal, bestIndex,
                        index, found);
        }
    }
    else if (fabsf(b) > CONE_EPSILON)
        tryConeBody(primitive, ray, -c / b, k2, bestDistance, bestNormal, bestIndex, index, found);

    const float baseY = primitive.center.y + (primitive.coneBaseAbove ? primitive.height : -primitive.height);
    const Vec3 baseNormal = makeVec3(0.0f, primitive.coneBaseAbove ? 1.0f : -1.0f, 0.0f);

    if (fabsf(ray.direction.y) > CONE_EPSILON)
    {
        const float distance = (baseY - ray.origin.y) / ray.direction.y;

        if (distance > RAY_EPSILON && distance < bestDistance)
        {
            const Vec3 hitPosition = rayAt(ray, distance);
            const float dx = hitPosition.x - primitive.center.x;
            const float dz = hitPosition.z - primitive.center.z;

            if (dx * dx + dz * dz <= primitive.radius * primitive.radius)
            {
                bestDistance = distance;
                bestNormal = baseNormal;
                bestIndex = index;
                found = true;
            }
        }
    }

    if (!found)
        return false;

    if (dot(ray.direction, bestNormal) > 0.0f)
        bestNormal = negate(bestNormal);
    bestNormal = normalize(bestNormal);
    bestShadingNormal = bestNormal;

    const Vec3 hitPosition = rayAt(ray, bestDistance);
    const float relativeY = hitPosition.y - primitive.center.y;
    const float normalizedHeight = (primitive.coneBaseAbove ? relativeY : -relativeY) / primitive.height;

    const Vec3 radial = makeVec3(hitPosition.x - primitive.center.x, 0.0f, hitPosition.z - primitive.center.z);
    const Vec3 circumference = cross(radial, makeVec3(0.0f, 1.0f, 0.0f));

    bestTangent = lengthSquared(circumference) > 1e-12f ? normalize(circumference) : stableTangent(bestNormal);

    bestU = 0.5f + atan2f(hitPosition.z - primitive.center.z, hitPosition.x - primitive.center.x) /
                        (2.0f * PI_F);
    bestV = fmaxf(0.0f, fminf(1.0f, normalizedHeight));
    return true;
}

__device__ bool intersectTriangle(const Triangle &triangle, const Ray &ray, float &bestDistance, Vec3 &bestNormal,
                                  Vec3 &bestShadingNormal, Vec3 &bestTangent, float &bestU, float &bestV, int &bestMaterialGroup,
                                  int &bestIndex, int index)
{
    const Vec3 edge1 = sub(triangle.v1, triangle.v0);
    const Vec3 edge2 = sub(triangle.v2, triangle.v0);
    const Vec3 pvec = cross(ray.direction, edge2);
    const float determinant = dot(edge1, pvec);

    if (!isfinite(determinant) || fabsf(determinant) < 1e-9f)
        return false;

    const float inverseDeterminant = 1.0f / determinant;
    const Vec3 tvec = sub(ray.origin, triangle.v0);
    const float u = dot(tvec, pvec) * inverseDeterminant;

    if (u < 0.0f || u > 1.0f)
        return false;

    const Vec3 qvec = cross(tvec, edge1);
    const float v = dot(ray.direction, qvec) * inverseDeterminant;

    if (v < 0.0f || u + v > 1.0f)
        return false;

    const float distance = dot(edge2, qvec) * inverseDeterminant;

    if (distance <= RAY_EPSILON || distance >= bestDistance)
        return false;

    Vec3 normal = cross(edge1, edge2);
    const float w0 = 1.0f - u - v;
    Vec3 shadingNormal = add(add(scale(triangle.n0, w0), scale(triangle.n1, u)), scale(triangle.n2, v));
    const float textureU = triangle.tu0 * w0 + triangle.tu1 * u + triangle.tu2 * v;
    const float textureV = triangle.tv0 * w0 + triangle.tv1 * u + triangle.tv2 * v;

    if (!isfinite(lengthSquared(shadingNormal)) || lengthSquared(shadingNormal) < 1e-12f)
        shadingNormal = normal;

    if (dot(normal, ray.direction) > 0.0f)
        normal = negate(normal);
    if (dot(shadingNormal, ray.direction) > 0.0f)
        shadingNormal = negate(shadingNormal);

    bestDistance = distance;
    bestNormal = normalize(normal);
    bestShadingNormal = normalize(shadingNormal);
    bestTangent = triangle.tangent;
    bestU = textureU;
    bestV = textureV;
    bestMaterialGroup = triangle.materialGroup;
    bestIndex = index;
    return true;
}

__device__ bool intersectMesh(const SceneView &scene, const Primitive &primitive, const Ray &ray, float &bestDistance,
                              Vec3 &bestNormal, Vec3 &bestShadingNormal, Vec3 &bestTangent, float &bestU, float &bestV,
                              int &bestMaterialIndex, int &bestIndex, int index)
{
    if (primitive.meshIndex < 0 || primitive.meshIndex >= scene.meshCount)
        return false;

    const MeshRange &range = scene.meshRanges[primitive.meshIndex];

    if (range.triangleCount <= 0)
        return false;

    bool hit = false;
    int bestMaterialGroup = -1;

    if (range.nodeCount <= 0 || range.indexCount <= 0)
    {
        for (int i = 0; i < range.triangleCount; ++i)
        {
            if (intersectTriangle(scene.meshTriangles[range.triangleStart + i], ray, bestDistance, bestNormal,
                                  bestShadingNormal, bestTangent, bestU, bestV, bestMaterialGroup, bestIndex, index))
            {
                bestMaterialIndex = -1;

                if (bestMaterialGroup >= 0 && bestMaterialGroup < primitive.meshMaterialMapCount)
                {
                    const int mapIndex = primitive.meshMaterialMapStart + bestMaterialGroup;

                    if (mapIndex >= 0 && mapIndex < scene.meshMaterialMapCount)
                        bestMaterialIndex = scene.meshMaterialMaps[mapIndex];
                }
                hit = true;
            }
        }
        return hit;
    }

    int stack[BVH_STACK_SIZE];
    int stackSize = 0;

    stack[stackSize++] = range.nodeStart;

    while (stackSize > 0)
    {
        const int nodeIndex = stack[--stackSize];
        const BVHNode &node = scene.meshNodes[nodeIndex];

        if (!intersectBounds(node, ray, bestDistance))
            continue;

        if (node.leaf)
        {
            for (int i = 0; i < node.count; ++i)
            {
                const int triangleIndex = scene.meshIndices[node.start + i];

                if (triangleIndex < 0 || triangleIndex >= scene.meshTriangleCount)
                    continue;

                if (intersectTriangle(scene.meshTriangles[triangleIndex], ray, bestDistance, bestNormal,
                                      bestShadingNormal, bestTangent, bestU, bestV, bestMaterialGroup, bestIndex,
                                      index))
                {
                    bestMaterialIndex = -1;

                    if (bestMaterialGroup >= 0 && bestMaterialGroup < primitive.meshMaterialMapCount)
                    {
                        const int mapIndex = primitive.meshMaterialMapStart + bestMaterialGroup;

                        if (mapIndex >= 0 && mapIndex < scene.meshMaterialMapCount)
                            bestMaterialIndex = scene.meshMaterialMaps[mapIndex];
                    }
                    hit = true;
                }
            }
        }
        else
            pushNodeChildren(scene.meshNodes, node, ray, bestDistance, stack, stackSize);
    }

    return hit;
}

__device__ inline Vec3 transformInversePoint(const TransformData &transform, const Vec3 &point)
{
    return makeVec3(transform.inverseLinear[0] * point.x + transform.inverseLinear[1] * point.y +
                        transform.inverseLinear[2] * point.z + transform.inverseTranslation[0],
                    transform.inverseLinear[3] * point.x + transform.inverseLinear[4] * point.y +
                        transform.inverseLinear[5] * point.z + transform.inverseTranslation[1],
                    transform.inverseLinear[6] * point.x + transform.inverseLinear[7] * point.y +
                        transform.inverseLinear[8] * point.z + transform.inverseTranslation[2]);
}

__device__ inline Vec3 transformInverseVector(const TransformData &transform, const Vec3 &vector)
{
    return makeVec3(transform.inverseLinear[0] * vector.x + transform.inverseLinear[1] * vector.y +
                        transform.inverseLinear[2] * vector.z,
                    transform.inverseLinear[3] * vector.x + transform.inverseLinear[4] * vector.y +
                        transform.inverseLinear[5] * vector.z,
                    transform.inverseLinear[6] * vector.x + transform.inverseLinear[7] * vector.y +
                        transform.inverseLinear[8] * vector.z);
}

__device__ inline Vec3 transformNormal(const TransformData &transform, const Vec3 &normal)
{
    return makeVec3(transform.inverseLinear[0] * normal.x + transform.inverseLinear[3] * normal.y +
                        transform.inverseLinear[6] * normal.z,
                    transform.inverseLinear[1] * normal.x + transform.inverseLinear[4] * normal.y +
                        transform.inverseLinear[7] * normal.z,
                    transform.inverseLinear[2] * normal.x + transform.inverseLinear[5] * normal.y +
                        transform.inverseLinear[8] * normal.z);
}

__device__ bool intersectBox(const Primitive &primitive, const Ray &ray, float &bestDistance, Vec3 &bestNormal,
                             Vec3 &bestShadingNormal, Vec3 &bestTangent, float &bestU, float &bestV, int &bestIndex, int index)
{
    const float origin[3] = {ray.origin.x, ray.origin.y, ray.origin.z};
    const float direction[3] = {ray.direction.x, ray.direction.y, ray.direction.z};
    const float minimum[3] = {primitive.localMin.x, primitive.localMin.y, primitive.localMin.z};
    const float maximum[3] = {primitive.localMax.x, primitive.localMax.y, primitive.localMax.z};
    float nearDistance = -3.402823466e38f;
    float farDistance = 3.402823466e38f;
    int nearAxis = -1;
    float nearSign = 1.0f;

    for (int axis = 0; axis < 3; ++axis)
    {
        if (fabsf(direction[axis]) < 1e-12f)
        {
            if (origin[axis] < minimum[axis] || origin[axis] > maximum[axis])
                return false;
            continue;
        }

        const float inverse = 1.0f / direction[axis];
        float first = (minimum[axis] - origin[axis]) * inverse;
        float second = (maximum[axis] - origin[axis]) * inverse;
        float sign = -1.0f;

        if (first > second)
        {
            const float temporary = first;

            first = second;
            second = temporary;
            sign = 1.0f;
        }
        if (first > nearDistance)
        {
            nearDistance = first;
            nearAxis = axis;
            nearSign = sign;
        }
        farDistance = fminf(farDistance, second);

        if (nearDistance > farDistance)
            return false;
    }

    if (nearAxis < 0)
        return false;

    float distance = nearDistance;

    if (distance <= RAY_EPSILON)
    {
        distance = farDistance;
        nearSign = -nearSign;
    }
    if (distance <= RAY_EPSILON || distance >= bestDistance)
        return false;

    const Vec3 hitPosition = rayAt(ray, distance);
    Vec3 normal = makeVec3(0.0f, 0.0f, 0.0f);

    if (nearAxis == 0)
        normal.x = nearSign;
    else if (nearAxis == 1)
        normal.y = nearSign;
    else
        normal.z = nearSign;

    if (dot(normal, ray.direction) > 0.0f)
        normal = negate(normal);

    bestDistance = distance;
    bestNormal = normalize(normal);
    bestShadingNormal = bestNormal;
    bestIndex = index;

    const Vec3 size = sub(primitive.localMax, primitive.localMin);
    const Vec3 local = sub(hitPosition, primitive.localMin);

    if (nearAxis == 0)
    {
        bestU = local.y / fmaxf(size.y, 1e-12f);
        bestV = local.z / fmaxf(size.z, 1e-12f);
    }
    else if (nearAxis == 1)
    {
        bestU = local.x / fmaxf(size.x, 1e-12f);
        bestV = local.z / fmaxf(size.z, 1e-12f);
    }
    else
    {
        bestU = local.x / fmaxf(size.x, 1e-12f);
        bestV = local.y / fmaxf(size.y, 1e-12f);
    }

    bestU = clamp01(bestU);
    bestV = clamp01(bestV);
    bestTangent = nearAxis == 0 ? makeVec3(0.0f, 1.0f, 0.0f) : makeVec3(1.0f, 0.0f, 0.0f);
    return true;
}

__device__ bool intersectDisk(const Primitive &primitive, const Ray &ray, float &bestDistance, Vec3 &bestNormal,
                              Vec3 &bestShadingNormal, Vec3 &bestTangent, float &bestU, float &bestV, int &bestIndex, int index)
{
    const float denominator = dot(ray.direction, primitive.normal);

    if (fabsf(denominator) < 1e-12f)
        return false;

    const float distance = dot(primitive.normal, sub(primitive.center, ray.origin)) / denominator;

    if (distance <= RAY_EPSILON || distance >= bestDistance)
        return false;

    const Vec3 hitPosition = rayAt(ray, distance);
    const Vec3 relative = sub(hitPosition, primitive.center);
    const float normalProjection = dot(relative, primitive.normal);
    const float radialSquared = fmaxf(lengthSquared(relative) - normalProjection * normalProjection, 0.0f);
    const float radiusSquared = primitive.radius * primitive.radius;
    const float innerSquared = primitive.innerRadius * primitive.innerRadius;

    if (radialSquared > radiusSquared || radialSquared < innerSquared)
        return false;

    Vec3 normal = primitive.normal;

    if (dot(normal, ray.direction) > 0.0f)
        normal = negate(normal);

    bestDistance = distance;
    bestNormal = normal;
    bestShadingNormal = normal;
    bestIndex = index;

    const Vec3 tangent = stableTangent(primitive.normal);
    const Vec3 bitangent = cross(primitive.normal, tangent);

    bestTangent = tangent;
    bestU = clamp01(0.5f + dot(relative, tangent) / primitive.radius * 0.5f);
    bestV = clamp01(0.5f + dot(relative, bitangent) / primitive.radius * 0.5f);
    return true;
}

__device__ bool intersectPrimitiveLocal(const SceneView &scene, const Primitive &primitive, const Ray &ray,
                                        float &bestDistance, Vec3 &bestNormal, Vec3 &bestShadingNormal, Vec3 &bestTangent, float &bestU,
                                        float &bestV, int &bestMaterialIndex, int &bestIndex, int index)
{
    bestMaterialIndex = -1;

    switch (primitive.type)
    {
    case PRIMITIVE_SPHERE:
        return intersectSphere(primitive, ray, bestDistance, bestNormal, bestShadingNormal, bestTangent, bestU, bestV, bestIndex,
                               index);
    case PRIMITIVE_PLANE:
        return intersectPlane(primitive, ray, bestDistance, bestNormal, bestShadingNormal, bestTangent, bestU, bestV, bestIndex,
                              index);
    case PRIMITIVE_CYLINDER:
        return intersectCylinder(primitive, ray, bestDistance, bestNormal, bestShadingNormal, bestTangent, bestU, bestV, bestIndex,
                                 index);
    case PRIMITIVE_CONE:
        return intersectCone(primitive, ray, bestDistance, bestNormal, bestShadingNormal, bestTangent, bestU, bestV, bestIndex,
                             index);
    case PRIMITIVE_MESH:
        return intersectMesh(scene, primitive, ray, bestDistance, bestNormal, bestShadingNormal, bestTangent, bestU, bestV,
                             bestMaterialIndex, bestIndex, index);
    case PRIMITIVE_BOX:
        return intersectBox(primitive, ray, bestDistance, bestNormal, bestShadingNormal, bestTangent, bestU, bestV, bestIndex,
                            index);
    case PRIMITIVE_DISK:
        return intersectDisk(primitive, ray, bestDistance, bestNormal, bestShadingNormal, bestTangent, bestU, bestV, bestIndex,
                             index);
    default:
        return false;
    }
}

__device__ bool intersectPrimitive(const SceneView &scene, const Primitive &primitive, const Ray &ray,
                                   float &bestDistance, Vec3 &bestNormal, Vec3 &bestShadingNormal, Vec3 &bestTangent, float &bestU,
                                   float &bestV, int &bestMaterialIndex, int &bestIndex, int index)
{
    if (!primitive.transformed)
    {
        // Test through a local override so a primitive that misses cannot
        // clear the override belonging to an earlier, successful mesh hit.
        int localMaterialIndex = -1;
        int localIndex = index;

        if (!intersectPrimitiveLocal(scene, primitive, ray, bestDistance, bestNormal, bestShadingNormal, bestTangent,
                                     bestU, bestV, localMaterialIndex, localIndex, index))
            return false;

        bestMaterialIndex = localMaterialIndex;
        bestIndex = localIndex;
        bestTangent = orthonormalizeTangent(bestTangent, bestShadingNormal);
        return true;
    }

    const Vec3 localDirection = transformInverseVector(primitive.transform, ray.direction);
    const float directionLength = length(localDirection);

    if (!isfinite(directionLength) || directionLength <= 0.0f)
        return false;

    Ray local;

    local.origin = transformInversePoint(primitive.transform, ray.origin);
    local.direction = scale(localDirection, 1.0f / directionLength);

    // Distances scale linearly with the ray direction, so the current best
    // distance must be converted into the normalized local space.
    float localDistance = bestDistance * directionLength;
    Vec3 localNormal = makeVec3(0.0f, 0.0f, 0.0f);
    Vec3 localShadingNormal = makeVec3(0.0f, 0.0f, 0.0f);
    Vec3 localTangent = makeVec3(0.0f, 0.0f, 0.0f);
    float localU = 0.0f;
    float localV = 0.0f;
    int localMaterialIndex = -1;
    int localIndex = index;

    if (!intersectPrimitiveLocal(scene, primitive, local, localDistance, localNormal, localShadingNormal,
                                 localTangent, localU, localV, localMaterialIndex, localIndex, index))
        return false;

    bestDistance = localDistance / directionLength;
    bestNormal = transformNormal(primitive.transform, localNormal);
    bestShadingNormal = transformNormal(primitive.transform, localShadingNormal);
    bestTangent = transformDirection(primitive.transform, localTangent);
    bestU = localU;
    bestV = localV;
    bestMaterialIndex = localMaterialIndex;

    if (!isfinite(lengthSquared(bestShadingNormal)) || lengthSquared(bestShadingNormal) < 1e-12f)
        bestShadingNormal = bestNormal;

    if (dot(ray.direction, bestNormal) > 0.0f)
        bestNormal = negate(bestNormal);
    if (dot(ray.direction, bestShadingNormal) > 0.0f)
        bestShadingNormal = negate(bestShadingNormal);

    bestNormal = normalize(bestNormal);
    bestShadingNormal = normalize(bestShadingNormal);
    bestTangent = orthonormalizeTangent(bestTangent, bestShadingNormal);
    bestIndex = index;
    return true;
}

__device__ bool intersectBounds(const BVHNode &node, const Ray &ray, float maxDistance)
{
    const float origin[3] = {ray.origin.x, ray.origin.y, ray.origin.z};
    const float direction[3] = {ray.direction.x, ray.direction.y, ray.direction.z};
    const float minimum[3] = {node.min.x, node.min.y, node.min.z};
    const float maximum[3] = {node.max.x, node.max.y, node.max.z};
    float nearDistance = 0.0f;
    float farDistance = maxDistance;

    for (int axis = 0; axis < 3; ++axis)
    {
        if (fabsf(direction[axis]) < DIRECTION_EPSILON)
        {
            if (origin[axis] < minimum[axis] || origin[axis] > maximum[axis])
                return false;
            continue;
        }

        float first = (minimum[axis] - origin[axis]) / direction[axis];
        float second = (maximum[axis] - origin[axis]) / direction[axis];

        if (first > second)
        {
            const float swap = first;
            first = second;
            second = swap;
        }

        nearDistance = fmaxf(nearDistance, first);
        farDistance = fminf(farDistance, second);

        if (nearDistance > farDistance)
            return false;
    }

    return true;
}

__device__ float boundsEntryDistance(const BVHNode &node, const Ray &ray)
{
    const float origin[3] = {ray.origin.x, ray.origin.y, ray.origin.z};
    const float direction[3] = {ray.direction.x, ray.direction.y, ray.direction.z};
    const float minimum[3] = {node.min.x, node.min.y, node.min.z};
    const float maximum[3] = {node.max.x, node.max.y, node.max.z};
    float nearDistance = 0.0f;
    float farDistance = INFINITY;

    for (int axis = 0; axis < 3; ++axis)
    {
        if (fabsf(direction[axis]) < DIRECTION_EPSILON)
        {
            if (origin[axis] < minimum[axis] || origin[axis] > maximum[axis])
                return INFINITY;
            continue;
        }

        float first = (minimum[axis] - origin[axis]) / direction[axis];
        float second = (maximum[axis] - origin[axis]) / direction[axis];

        if (first > second)
        {
            const float swap = first;
            first = second;
            second = swap;
        }

        nearDistance = fmaxf(nearDistance, first);
        farDistance = fminf(farDistance, second);

        if (nearDistance > farDistance)
            return INFINITY;
    }

    return nearDistance;
}

__device__ void pushNodeChildren(const BVHNode *nodes, const BVHNode &node, const Ray &ray, float maxDistance,
                                 int *stack, int &stackSize)
{
    if (node.left < 0 || node.right < 0)
        return;

    const BVHNode &leftNode = nodes[node.left];
    const BVHNode &rightNode = nodes[node.right];
    const float leftDistance = boundsEntryDistance(leftNode, ray);
    const float rightDistance = boundsEntryDistance(rightNode, ray);
    const int nearNode = leftDistance <= rightDistance ? node.left : node.right;
    const int farNode = leftDistance <= rightDistance ? node.right : node.left;
    const float nearDistance = leftDistance <= rightDistance ? leftDistance : rightDistance;
    const float farDistance = leftDistance <= rightDistance ? rightDistance : leftDistance;

    // Defensive bound check: the builder keeps the tree shallow, but an
    // out-of-bounds write here would corrupt device state.
    if (stackSize + 2 > BVH_STACK_SIZE)
        return;

    // Push the far child first so the near child is popped (and tested) first.
    if (farDistance < maxDistance)
        stack[stackSize++] = farNode;
    if (nearDistance < maxDistance)
        stack[stackSize++] = nearNode;
}

__device__ void pushChildren(const SceneView &scene, const BVHNode &node, const Ray &ray, float maxDistance,
                             int *stack, int &stackSize)
{
    pushNodeChildren(scene.nodes, node, ray, maxDistance, stack, stackSize);
}

__device__ bool intersectBVH(const SceneView &scene, const Ray &ray, float &distance, Vec3 &normal,
                             Vec3 &shadingNormal, Vec3 &tangent, float &u, float &v, int &materialIndex, int &index)
{
    int stack[BVH_STACK_SIZE];
    int stackSize = 0;
    bool hit = false;

    stack[stackSize++] = 0;

    while (stackSize > 0)
    {
        const int nodeIndex = stack[--stackSize];
        const BVHNode &node = scene.nodes[nodeIndex];

        if (!intersectBounds(node, ray, distance))
            continue;

        if (node.leaf)
        {
            for (int i = 0; i < node.count; ++i)
            {
                const int primitiveIndex = scene.primitiveIndices[node.start + i];

                if (intersectPrimitive(scene, scene.primitives[primitiveIndex], ray, distance, normal, shadingNormal,
                                       tangent, u, v, materialIndex, index, primitiveIndex))
                    hit = true;
            }
        }
        else
            pushChildren(scene, node, ray, distance, stack, stackSize);
    }

    return hit;
}

__device__ bool intersectScene(const SceneView &scene, const Ray &ray, float &distance, Vec3 &normal,
                               Vec3 &shadingNormal, Vec3 &tangent, float &u, float &v, int &materialIndex,
                               int &index)
{
    distance = INFINITY;
    normal = makeVec3(0.0f, 0.0f, 0.0f);
    shadingNormal = makeVec3(0.0f, 0.0f, 0.0f);
    tangent = makeVec3(0.0f, 0.0f, 0.0f);
    u = 0.0f;
    v = 0.0f;
    materialIndex = -1;
    index = -1;
    bool hit = false;

    if (scene.nodeCount > 0)
        hit = intersectBVH(scene, ray, distance, normal, shadingNormal, tangent, u, v, materialIndex, index);
    else
    {
        for (int i = 0; i < scene.primitiveCount; ++i)
        {
            if (intersectPrimitive(scene, scene.primitives[i], ray, distance, normal, shadingNormal, tangent, u, v,
                                   materialIndex, index, i))
                hit = true;
        }
    }

    for (int i = 0; i < scene.unboundedCount; ++i)
    {
        const int primitiveIndex = scene.unboundedIndices[i];

        if (intersectPrimitive(scene, scene.primitives[primitiveIndex], ray, distance, normal, shadingNormal, tangent,
                               u, v, materialIndex, index, primitiveIndex))
            hit = true;
    }

    return hit;
}

__device__ bool occluded(const SceneView &scene, const Ray &ray, float maxDistance)
{
    float distance = maxDistance;
    Vec3 normal = makeVec3(0.0f, 0.0f, 0.0f);
    Vec3 shadingNormal = makeVec3(0.0f, 0.0f, 0.0f);
    Vec3 tangent = makeVec3(0.0f, 0.0f, 0.0f);
    float u = 0.0f;
    float v = 0.0f;
    int materialIndex = -1;
    int index = -1;

    if (scene.nodeCount > 0)
    {
        int stack[BVH_STACK_SIZE];
        int stackSize = 0;

        stack[stackSize++] = 0;

        while (stackSize > 0)
        {
            const int nodeIndex = stack[--stackSize];
            const BVHNode &node = scene.nodes[nodeIndex];

            if (!intersectBounds(node, ray, distance))
                continue;

            if (node.leaf)
            {
                for (int i = 0; i < node.count; ++i)
                {
                    const int primitiveIndex = scene.primitiveIndices[node.start + i];

                    if (intersectPrimitive(scene, scene.primitives[primitiveIndex], ray, distance, normal,
                                           shadingNormal, tangent, u, v, materialIndex, index, primitiveIndex))
                        return true;
                }
            }
            else
                pushChildren(scene, node, ray, distance, stack, stackSize);
        }
    }
    else
    {
        for (int i = 0; i < scene.primitiveCount; ++i)
        {
            if (intersectPrimitive(scene, scene.primitives[i], ray, distance, normal, shadingNormal, tangent, u, v,
                                   materialIndex, index, i))
                return true;
        }
    }

    for (int i = 0; i < scene.unboundedCount; ++i)
    {
        const int primitiveIndex = scene.unboundedIndices[i];

        if (intersectPrimitive(scene, scene.primitives[primitiveIndex], ray, distance, normal, shadingNormal, tangent,
                               u, v, materialIndex, index, primitiveIndex))
            return true;
    }

    return false;
}

struct LightSample
{
    Vec3 direction;
    Vec3 radiance;
    float distance;
    bool valid;
};

} // namespace
} // namespace Cuda
} // namespace RayTracer

#endif // CUDA_INTERSECTIONS_CUH_
