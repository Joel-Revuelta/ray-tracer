/*
** EPITECH PROJECT, 2024
** raytracer
** File description:
** CUDA kernels
*/

#include "plugins/renderers/CUDA/CudaKernels.hpp"
#include <cstddef>
#include <cuda_runtime.h>
#include <math.h>

namespace RayTracer
{
namespace Cuda
{
namespace
{
constexpr float RAY_EPSILON = 0.0001f;
constexpr float DIRECTION_EPSILON = 1e-12f;
constexpr float AXIS_EPSILON = 1e-6f;
constexpr float CONE_EPSILON = 1e-6f;
constexpr float PI_F = 3.14159265358979323846f;
constexpr int MAX_BOUNCES = 15;
// The shared builder caps SAH depth and falls back to median splits, so the
// traversal stack only needs to cover a balanced tree plus the SAH prefix.
constexpr int BVH_STACK_SIZE = 128;

__device__ inline Vec3 makeVec3(float x, float y, float z)
{
    Vec3 result = {x, y, z};
    return result;
}

__device__ inline Vec3 add(const Vec3 &a, const Vec3 &b)
{
    return makeVec3(a.x + b.x, a.y + b.y, a.z + b.z);
}

__device__ inline Vec3 sub(const Vec3 &a, const Vec3 &b)
{
    return makeVec3(a.x - b.x, a.y - b.y, a.z - b.z);
}

__device__ inline Vec3 scale(const Vec3 &v, float factor)
{
    return makeVec3(v.x * factor, v.y * factor, v.z * factor);
}

__device__ inline Vec3 multiply(const Vec3 &a, const Vec3 &b)
{
    return makeVec3(a.x * b.x, a.y * b.y, a.z * b.z);
}

__device__ inline Vec3 negate(const Vec3 &v)
{
    return makeVec3(-v.x, -v.y, -v.z);
}

__device__ inline float dot(const Vec3 &a, const Vec3 &b)
{
    return a.x * b.x + a.y * b.y + a.z * b.z;
}

__device__ inline float lengthSquared(const Vec3 &v)
{
    return dot(v, v);
}

__device__ inline float length(const Vec3 &v)
{
    return sqrtf(lengthSquared(v));
}

__device__ inline Vec3 normalize(const Vec3 &v)
{
    const float lenSquared = lengthSquared(v);

    if (!isfinite(lenSquared) || lenSquared < 1e-12f)
        return makeVec3(0.0f, 0.0f, 0.0f);
    return scale(v, 1.0f / sqrtf(lenSquared));
}

__device__ inline Vec3 reflect(const Vec3 &incident, const Vec3 &normal)
{
    return sub(incident, scale(normal, 2.0f * dot(incident, normal)));
}

__device__ inline float clamp01(float value)
{
    return fminf(fmaxf(value, 0.0f), 1.0f);
}

__device__ inline float linearToSRGB(float value)
{
    const float clamped = clamp01(value);

    if (clamped <= 0.0031308f)
        return 12.92f * clamped;
    return 1.055f * powf(clamped, 1.0f / 2.4f) - 0.055f;
}

__device__ Vec3 sampleTextureClamped(const SceneView &scene, int index, float u, float v);

__device__ inline Vec3 skyColor(const SceneView &scene, const Vec3 &direction)
{
    const Vec3 unit = normalize(direction);

    if (scene.background.environmentTextureIndex >= 0)
    {
        const float u = 0.5f + atan2f(unit.z, unit.x) / (2.0f * PI_F);
        const float v = acosf(fmaxf(-1.0f, fminf(1.0f, unit.y))) / PI_F;

        return scale(sampleTextureClamped(scene, scene.background.environmentTextureIndex, u, v),
                     scene.background.intensity);
    }

    const float vertical = fminf(fmaxf(unit.y, -1.0f), 1.0f);
    const float blend = sqrtf(fabsf(vertical));

    if (vertical >= 0.0f)
        return scale(add(scale(scene.background.horizon, 1.0f - blend), scale(scene.background.zenith, blend)),
                     scene.background.intensity);
    return scale(add(scale(scene.background.horizon, 1.0f - blend), scale(scene.background.ground, blend)),
                 scene.background.intensity);
}

__device__ inline float postProcessChannel(float linear, const PostProcess &settings)
{
    // Match the CPU byte conversion: non-finite radiance becomes black rather
    // than a saturated or undefined pixel.
    if (!isfinite(linear))
        return 0.0f;

    const float exposed = fmaxf(linear * settings.exposure, 0.0f);
    float tonemapped = exposed;

    if (settings.toneMapping == 1)
        tonemapped = exposed / (1.0f + exposed);
    else if (settings.toneMapping == 2)
    {
        constexpr float a = 2.51f;
        constexpr float b = 0.03f;
        constexpr float c = 2.43f;
        constexpr float d = 0.59f;
        constexpr float e = 0.14f;

        tonemapped = (exposed * (a * exposed + b)) / (exposed * (c * exposed + d) + e);
    }

    if (settings.srgb != 0)
        tonemapped = linearToSRGB(tonemapped);

    return clamp01(tonemapped);
}

__device__ inline unsigned int pcgHash(unsigned int input)
{
    const unsigned int state = input * 747796405u + 2891336453u;
    const unsigned int word = ((state >> ((state >> 28u) + 4u)) ^ state) * 277803737u;

    return (word >> 22u) ^ word;
}

__device__ inline float randomFloat(unsigned int &state)
{
    state = pcgHash(state);
    return static_cast<float>(state) / 4294967295.0f;
}

__device__ inline Vec3 randomOnUnitSphere(unsigned int &state)
{
    while (true)
    {
        Vec3 value = makeVec3(randomFloat(state) * 2.0f - 1.0f, randomFloat(state) * 2.0f - 1.0f,
                              randomFloat(state) * 2.0f - 1.0f);
        const float lenSquared = lengthSquared(value);

        if (lenSquared >= 1.0f || lenSquared < 1e-12f)
            continue;
        return normalize(value);
    }
}

__device__ inline Vec3 randomCosineHemisphere(const Vec3 &normal, unsigned int &state)
{
    const Vec3 direction = add(randomOnUnitSphere(state), normal);
    const float lenSquared = lengthSquared(direction);

    if (!isfinite(lenSquared) || lenSquared < 1e-12f)
        return normal;
    return scale(direction, 1.0f / sqrtf(lenSquared));
}

__device__ inline Vec3 cross(const Vec3 &a, const Vec3 &b)
{
    return makeVec3(a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x);
}

__device__ inline float smithGGX(float ndot, float alpha)
{
    const float a2 = alpha * alpha;

    return (2.0f * ndot) / (ndot + sqrtf(a2 + (1.0f - a2) * ndot * ndot));
}

__device__ inline Vec3 fresnelSchlick(const Vec3 &f0, float cosTheta)
{
    const float factor = powf(fmaxf(1.0f - cosTheta, 0.0f), 5.0f);
    const Vec3 white = makeVec3(1.0f, 1.0f, 1.0f);

    return add(f0, scale(sub(white, f0), factor));
}

__device__ inline float blinnShininess(float roughness)
{
    if (roughness <= 1e-3f)
        return 64.0f;

    const float alpha = fmaxf(roughness, 1e-3f);
    const float exponent = 2.0f / (alpha * alpha * alpha * alpha) - 2.0f;

    return fminf(fmaxf(exponent, 1.0f), 4096.0f);
}

__device__ inline Vec3 sampleGGXHalfVector(const Vec3 &normal, float roughness, unsigned int &state)
{
    const float alpha = fmaxf(roughness * roughness, 1e-4f);
    const float u1 = randomFloat(state);
    const float u2 = randomFloat(state);
    const float phi = 2.0f * PI_F * u1;
    const float cosTheta = sqrtf((1.0f - u2) / (1.0f + (alpha * alpha - 1.0f) * u2));
    const float sinTheta = sqrtf(fmaxf(0.0f, 1.0f - cosTheta * cosTheta));
    const Vec3 tangent = normalize(cross(fabsf(normal.x) < 0.999f ? makeVec3(1.0f, 0.0f, 0.0f)
                                                                   : makeVec3(0.0f, 1.0f, 0.0f),
                                         normal));
    const Vec3 bitangent = cross(normal, tangent);

    return normalize(add(add(scale(tangent, cosf(phi) * sinTheta), scale(bitangent, sinf(phi) * sinTheta)),
                         scale(normal, cosTheta)));
}

__device__ inline bool sampleMetallic(const Vec3 &viewDirection, const Vec3 &normal, const Vec3 &f0,
                                      float roughness, unsigned int &state, Vec3 &direction, Vec3 &weight)
{
    if (roughness <= 1e-3f)
    {
        direction = normalize(reflect(negate(viewDirection), normal));
        weight = f0;
        return lengthSquared(direction) >= 1e-12f;
    }

    const Vec3 halfVector = sampleGGXHalfVector(normal, roughness, state);

    direction = normalize(reflect(negate(viewDirection), halfVector));

    const float ndotL = dot(normal, direction);
    const float ndotV = fmaxf(dot(normal, viewDirection), 1e-4f);
    const float ndotH = fmaxf(dot(normal, halfVector), 1e-4f);
    const float vdotH = fmaxf(dot(viewDirection, halfVector), 1e-4f);

    if (ndotL <= 0.0f)
        return false;

    const float alpha = fmaxf(roughness * roughness, 1e-4f);
    const Vec3 fresnel = fresnelSchlick(f0, vdotH);
    const float geometry = smithGGX(ndotV, alpha) * smithGGX(ndotL, alpha);
    const float factor = fminf(fmaxf(geometry * vdotH / (ndotV * ndotH), 0.0f), 1.0f);

    weight = scale(fresnel, factor);
    return lengthSquared(weight) >= 1e-12f;
}

struct Ray
{
    Vec3 origin;
    Vec3 direction;
};

__device__ inline Vec3 rayAt(const Ray &ray, float distance)
{
    return add(ray.origin, scale(ray.direction, distance));
}

__device__ bool intersectBounds(const BVHNode &node, const Ray &ray, float maxDistance);
__device__ void pushNodeChildren(const BVHNode *nodes, const BVHNode &node, const Ray &ray, float maxDistance,
                                 int *stack, int &stackSize);

__device__ bool intersectSphere(const Primitive &primitive, const Ray &ray, float &bestDistance, Vec3 &bestNormal,
                                Vec3 &bestShadingNormal, float &bestU, float &bestV, int &bestIndex, int index)
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

    bestU = 0.5f + atan2f(local.z, local.x) / (2.0f * PI_F);
    bestV = acosf(fmaxf(-1.0f, fminf(1.0f, local.y))) / PI_F;
    return true;
}

__device__ bool intersectPlane(const Primitive &primitive, const Ray &ray, float &bestDistance, Vec3 &bestNormal,
                               Vec3 &bestShadingNormal, float &bestU, float &bestV, int &bestIndex, int index)
{
    const float denominator = dot(primitive.normal, ray.direction);

    if (fabsf(denominator) < RAY_EPSILON)
        return false;

    const float distance = dot(primitive.normal, sub(primitive.point, ray.origin)) / denominator;

    if (distance < 0.0f || distance >= bestDistance)
        return false;

    bestDistance = distance;
    bestIndex = index;
    bestNormal = denominator > 0.0f ? negate(primitive.normal) : primitive.normal;
    bestShadingNormal = bestNormal;

    const Vec3 hitPosition = rayAt(ray, distance);

    bestU = hitPosition.x;
    bestV = hitPosition.z;
    return true;
}

__device__ bool intersectCylinder(const Primitive &primitive, const Ray &ray, float &bestDistance, Vec3 &bestNormal,
                                  Vec3 &bestShadingNormal, float &bestU, float &bestV, int &bestIndex, int index)
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

                if (distance < 0.0f || distance >= bestDistance)
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

        if (distance < 0.0f || distance >= bestDistance)
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

    bestU = 0.5f + atan2f(radial.z, radial.x) / (2.0f * PI_F);
    bestV = fmaxf(0.0f, fminf(1.0f, heightAtHit / primitive.height));
    return true;
}

__device__ void tryConeBody(const Primitive &primitive, const Ray &ray, float distance, float k2, float &bestDistance,
                            Vec3 &bestNormal, int &bestIndex, int index, bool &found)
{
    if (distance < 0.0f || distance >= bestDistance)
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
                              Vec3 &bestShadingNormal, float &bestU, float &bestV, int &bestIndex, int index)
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

        if (distance >= 0.0f && distance < bestDistance)
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

    bestU = 0.5f + atan2f(hitPosition.z - primitive.center.z, hitPosition.x - primitive.center.x) /
                        (2.0f * PI_F);
    bestV = fmaxf(0.0f, fminf(1.0f, normalizedHeight));
    return true;
}

__device__ bool intersectTriangle(const Triangle &triangle, const Ray &ray, float &bestDistance, Vec3 &bestNormal,
                                  Vec3 &bestShadingNormal, float &bestU, float &bestV, int &bestMaterialGroup,
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
    bestU = textureU;
    bestV = textureV;
    bestMaterialGroup = triangle.materialGroup;
    bestIndex = index;
    return true;
}

__device__ bool intersectMesh(const SceneView &scene, const Primitive &primitive, const Ray &ray, float &bestDistance,
                              Vec3 &bestNormal, Vec3 &bestShadingNormal, float &bestU, float &bestV,
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
                                  bestShadingNormal, bestU, bestV, bestMaterialGroup, bestIndex, index))
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
                                      bestShadingNormal, bestU, bestV, bestMaterialGroup, bestIndex, index))
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

__device__ bool intersectPrimitiveLocal(const SceneView &scene, const Primitive &primitive, const Ray &ray,
                                        float &bestDistance, Vec3 &bestNormal, Vec3 &bestShadingNormal, float &bestU,
                                        float &bestV, int &bestMaterialIndex, int &bestIndex, int index)
{
    bestMaterialIndex = -1;

    switch (primitive.type)
    {
    case PRIMITIVE_SPHERE:
        return intersectSphere(primitive, ray, bestDistance, bestNormal, bestShadingNormal, bestU, bestV, bestIndex,
                               index);
    case PRIMITIVE_PLANE:
        return intersectPlane(primitive, ray, bestDistance, bestNormal, bestShadingNormal, bestU, bestV, bestIndex,
                              index);
    case PRIMITIVE_CYLINDER:
        return intersectCylinder(primitive, ray, bestDistance, bestNormal, bestShadingNormal, bestU, bestV, bestIndex,
                                 index);
    case PRIMITIVE_CONE:
        return intersectCone(primitive, ray, bestDistance, bestNormal, bestShadingNormal, bestU, bestV, bestIndex,
                             index);
    case PRIMITIVE_MESH:
        return intersectMesh(scene, primitive, ray, bestDistance, bestNormal, bestShadingNormal, bestU, bestV,
                             bestMaterialIndex, bestIndex, index);
    default:
        return false;
    }
}

__device__ bool intersectPrimitive(const SceneView &scene, const Primitive &primitive, const Ray &ray,
                                   float &bestDistance, Vec3 &bestNormal, Vec3 &bestShadingNormal, float &bestU,
                                   float &bestV, int &bestMaterialIndex, int &bestIndex, int index)
{
    if (!primitive.transformed)
    {
        // Test through a local override so a primitive that misses cannot
        // clear the override belonging to an earlier, successful mesh hit.
        int localMaterialIndex = -1;
        int localIndex = index;

        if (!intersectPrimitiveLocal(scene, primitive, ray, bestDistance, bestNormal, bestShadingNormal, bestU, bestV,
                                     localMaterialIndex, localIndex, index))
            return false;

        bestMaterialIndex = localMaterialIndex;
        bestIndex = localIndex;
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
    float localU = 0.0f;
    float localV = 0.0f;
    int localMaterialIndex = -1;
    int localIndex = index;

    if (!intersectPrimitiveLocal(scene, primitive, local, localDistance, localNormal, localShadingNormal, localU, localV,
                                 localMaterialIndex, localIndex, index))
        return false;

    bestDistance = localDistance / directionLength;
    bestNormal = transformNormal(primitive.transform, localNormal);
    bestShadingNormal = transformNormal(primitive.transform, localShadingNormal);
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
                             Vec3 &shadingNormal, float &u, float &v, int &materialIndex, int &index)
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

                if (intersectPrimitive(scene, scene.primitives[primitiveIndex], ray, distance, normal, shadingNormal, u, v, materialIndex, index, primitiveIndex))
                    hit = true;
            }
        }
        else
            pushChildren(scene, node, ray, distance, stack, stackSize);
    }

    return hit;
}

__device__ bool intersectScene(const SceneView &scene, const Ray &ray, float &distance, Vec3 &normal,
                               Vec3 &shadingNormal, float &u, float &v, int &materialIndex, int &index)
{
    distance = INFINITY;
    normal = makeVec3(0.0f, 0.0f, 0.0f);
    shadingNormal = makeVec3(0.0f, 0.0f, 0.0f);
    u = 0.0f;
    v = 0.0f;
    materialIndex = -1;
    index = -1;
    bool hit = false;

    if (scene.nodeCount > 0)
        hit = intersectBVH(scene, ray, distance, normal, shadingNormal, u, v, materialIndex, index);
    else
    {
        for (int i = 0; i < scene.primitiveCount; ++i)
        {
            if (intersectPrimitive(scene, scene.primitives[i], ray, distance, normal, shadingNormal, u, v, materialIndex, index, i))
                hit = true;
        }
    }

    for (int i = 0; i < scene.unboundedCount; ++i)
    {
        const int primitiveIndex = scene.unboundedIndices[i];

        if (intersectPrimitive(scene, scene.primitives[primitiveIndex], ray, distance, normal, shadingNormal, u, v, materialIndex, index, primitiveIndex))
            hit = true;
    }

    return hit;
}

__device__ bool occluded(const SceneView &scene, const Ray &ray, float maxDistance)
{
    float distance = maxDistance;
    Vec3 normal = makeVec3(0.0f, 0.0f, 0.0f);
    Vec3 shadingNormal = makeVec3(0.0f, 0.0f, 0.0f);
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

                    if (intersectPrimitive(scene, scene.primitives[primitiveIndex], ray, distance, normal, shadingNormal, u, v, materialIndex, index,
                                           primitiveIndex))
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
            if (intersectPrimitive(scene, scene.primitives[i], ray, distance, normal, shadingNormal, u, v, materialIndex, index, i))
                return true;
        }
    }

    for (int i = 0; i < scene.unboundedCount; ++i)
    {
        const int primitiveIndex = scene.unboundedIndices[i];

        if (intersectPrimitive(scene, scene.primitives[primitiveIndex], ray, distance, normal, shadingNormal, u, v, materialIndex, index, primitiveIndex))
            return true;
    }

    return false;
}

struct LightSample
{
    Vec3 direction;
    Vec3 radiance;
    bool valid;
};

__device__ LightSample sampleLight(const SceneView &scene, const Light &light, const Vec3 &hitPosition,
                                   const Vec3 &hitNormal, const Vec3 &shadingNormal, unsigned int &state)
{
    LightSample sample{makeVec3(0.0f, 0.0f, 0.0f), makeVec3(0.0f, 0.0f, 0.0f), false};

    if (light.type == LIGHT_AMBIENT)
    {
        sample.radiance = scale(light.color, light.intensity);
        sample.valid = true;
        return sample;
    }

    if (light.type == LIGHT_DIRECTIONAL)
    {
        sample.direction = normalize(light.direction);

        if (dot(sample.direction, shadingNormal) <= 0.0f)
            return sample;
        if (occluded(scene, Ray{add(hitPosition, scale(hitNormal, RAY_EPSILON)), sample.direction}, INFINITY))
            return sample;

        sample.radiance = scale(light.color, light.intensity);
        sample.valid = true;
        return sample;
    }

    if (light.type == LIGHT_POINT)
    {
        const Vec3 toLight = sub(light.position, hitPosition);
        const float distance = length(toLight);

        sample.direction = normalize(toLight);

        if (dot(sample.direction, shadingNormal) <= 0.0f)
            return sample;

        const float denominator = light.attenuationConstant + light.attenuationLinear * distance +
                                  light.attenuationQuadratic * distance * distance;
        const float attenuation = denominator > 0.0f ? 1.0f / denominator : 0.0f;

        if (occluded(scene, Ray{add(hitPosition, scale(hitNormal, RAY_EPSILON)), sample.direction}, distance))
            return sample;

        sample.radiance = scale(light.color, light.intensity * attenuation);
        sample.valid = true;
        return sample;
    }

    if (light.type == LIGHT_AREA_SPHERE)
    {
        const float u = randomFloat(state);
        const float v = randomFloat(state);
        const float z = 1.0f - 2.0f * u;
        const float radial = sqrtf(fmaxf(0.0f, 1.0f - z * z));
        const float angle = 2.0f * PI_F * v;
        const Vec3 sphereNormal = makeVec3(radial * cosf(angle), radial * sinf(angle), z);
        const Vec3 lightPoint = add(light.position, scale(sphereNormal, light.radius));
        const Vec3 toLight = sub(lightPoint, hitPosition);
        const float distance = length(toLight);

        if (!isfinite(distance) || distance <= 1e-6f)
            return sample;

        sample.direction = scale(toLight, 1.0f / distance);

        if (dot(sample.direction, shadingNormal) <= 0.0f)
            return sample;

        const float denominator = light.attenuationConstant + light.attenuationLinear * distance +
                                  light.attenuationQuadratic * distance * distance;
        const float attenuation = denominator > 0.0f ? 1.0f / denominator : 0.0f;

        if (occluded(scene, Ray{add(hitPosition, scale(hitNormal, RAY_EPSILON)), sample.direction}, distance))
            return sample;

        sample.radiance = scale(light.color, light.intensity * attenuation);
        sample.valid = true;
        return sample;
    }

    return sample;
}

__device__ Vec3 textureTexel(const SceneView &scene, const TextureRange &range, int x, int y, bool clampY)
{
    x %= range.width;
    if (x < 0)
        x += range.width;

    if (clampY)
    {
        if (y < 0)
            y = 0;
        else if (y >= range.height)
            y = range.height - 1;
    }
    else
    {
        y %= range.height;
        if (y < 0)
            y += range.height;
    }

    return scene.texels[range.start + y * range.width + x];
}

__device__ Vec3 sampleTextureInternal(const SceneView &scene, int index, float u, float v, bool clampV)
{
    if (index < 0 || index >= scene.textureCount || scene.texels == nullptr || scene.textures == nullptr)
        return makeVec3(1.0f, 1.0f, 1.0f);

    const TextureRange &range = scene.textures[index];

    if (range.width <= 0 || range.height <= 0)
        return makeVec3(1.0f, 1.0f, 1.0f);

    u -= floorf(u);

    if (clampV)
        v = fmaxf(0.0f, fminf(1.0f, v));
    else
        v -= floorf(v);

    const float x = u * static_cast<float>(range.width) - 0.5f;
    const float y = v * static_cast<float>(range.height) - 0.5f;
    const int x0 = static_cast<int>(floorf(x));
    const int y0 = static_cast<int>(floorf(y));
    const float fx = x - static_cast<float>(x0);
    const float fy = y - static_cast<float>(y0);

    const Vec3 top = add(scale(textureTexel(scene, range, x0, y0, clampV), 1.0f - fx),
                         scale(textureTexel(scene, range, x0 + 1, y0, clampV), fx));
    const Vec3 bottom = add(scale(textureTexel(scene, range, x0, y0 + 1, clampV), 1.0f - fx),
                            scale(textureTexel(scene, range, x0 + 1, y0 + 1, clampV), fx));

    return add(scale(top, 1.0f - fy), scale(bottom, fy));
}

__device__ Vec3 sampleTexture(const SceneView &scene, int index, float u, float v)
{
    return sampleTextureInternal(scene, index, u, v, false);
}

__device__ Vec3 sampleTextureClamped(const SceneView &scene, int index, float u, float v)
{
    return sampleTextureInternal(scene, index, u, v, true);
}

__device__ Vec3 traceSample(const SceneView &scene, int x, int y, int sample)
{
    unsigned int state =
        pcgHash(static_cast<unsigned int>(x) + 0x9e3779b9u * static_cast<unsigned int>(y) +
                static_cast<unsigned int>(sample + 1) + scene.seed);
    const Vec3 pixelPosition =
        add(add(scene.camera.initialPixelPos, scale(scene.camera.pixelWidthDistance, static_cast<float>(x))),
            scale(scene.camera.pixelHeightDistance, static_cast<float>(y)));
    Vec3 direction = sub(pixelPosition, scene.camera.lookFrom);

    // Jitter in pixel space so anti-aliasing does not depend on scene scale or
    // camera distance.
    const float jitterX = randomFloat(state) - 0.5f;
    const float jitterY = randomFloat(state) - 0.5f;

    direction = add(direction, add(scale(scene.camera.pixelWidthDistance, jitterX),
                                   scale(scene.camera.pixelHeightDistance, jitterY)));

    Vec3 origin = scene.camera.lookFrom;

    // Match the CPU branch exactly: a zero lens radius must not consume random
    // values, so pinhole renders remain identical.
    if (scene.camera.lensRadius > 0.0f && scene.camera.focalDistance > 0.0f)
    {
        const float lensU = randomFloat(state);
        const float lensV = randomFloat(state);
        const float lensDistance = sqrtf(lensU);
        const float lensAngle = 2.0f * PI_F * lensV;

        origin = add(origin, add(scale(scene.camera.viewRight,
                                       lensDistance * cosf(lensAngle) * scene.camera.lensRadius),
                                 scale(scene.camera.viewUp,
                                       lensDistance * sinf(lensAngle) * scene.camera.lensRadius)));

        const float focusRatio = scene.camera.focusDistance / scene.camera.focalDistance;

        direction = sub(add(scene.camera.lookFrom, scale(direction, focusRatio)), origin);
    }

    Ray ray{origin, direction};
    Vec3 accumulated = makeVec3(0.0f, 0.0f, 0.0f);
    Vec3 throughput = makeVec3(1.0f, 1.0f, 1.0f);
    bool insideDielectric = false;

    for (int bounce = 0; bounce < MAX_BOUNCES; ++bounce)
    {
        float distance = 0.0f;
        Vec3 normal = makeVec3(0.0f, 0.0f, 0.0f);
        Vec3 shadingNormal = makeVec3(0.0f, 0.0f, 0.0f);
        float u = 0.0f;
        float v = 0.0f;
        int materialIndex = -1;
        int primitiveIndex = -1;

        if (!intersectScene(scene, ray, distance, normal, shadingNormal, u, v, materialIndex, primitiveIndex))
        {
            if (scene.background.enabled != 0)
                accumulated = add(accumulated, multiply(throughput, skyColor(scene, ray.direction)));
            break;
        }

        const Primitive &primitive = scene.primitives[primitiveIndex];

        const unsigned int resolvedMaterial =
            materialIndex >= 0 && materialIndex < scene.materialCount ? static_cast<unsigned int>(materialIndex)
                                                                      : primitive.materialIndex;

        if (resolvedMaterial >= static_cast<unsigned int>(scene.materialCount))
            break;

        const Material &material = scene.materials[resolvedMaterial];
        const Vec3 hitPosition = rayAt(ray, distance);
        const Vec3 surfaceColor = multiply(material.diffuseColor, sampleTexture(scene, material.textureIndex, u, v));

        if (material.type == MATERIAL_DIELECTRIC)
        {
            // Camera rays are not normalized. Fresnel and Snell's law require
            // a unit incident vector.
            const Vec3 incident = normalize(ray.direction);

            if (lengthSquared(incident) < 1e-12f)
                break;

            const float cosi = clamp01(-dot(incident, normal));
            const float etaI = insideDielectric ? material.ior : 1.0f;
            const float etaT = insideDielectric ? 1.0f : material.ior;
            const float eta = etaI / etaT;
            const float k = 1.0f - eta * eta * (1.0f - cosi * cosi);
            const float ratio = (etaI - etaT) / (etaI + etaT);
            const float reflectance = ratio * ratio;
            const float fresnel = reflectance + (1.0f - reflectance) * powf(1.0f - cosi, 5.0f);
            const bool reflectRay = k < 0.0f || randomFloat(state) < fresnel;
            Vec3 nextDirection;

            if (reflectRay)
                nextDirection = normalize(reflect(incident, normal));
            else
            {
                nextDirection = normalize(add(scale(incident, eta), scale(normal, eta * cosi - sqrtf(k))));
                insideDielectric = !insideDielectric;
            }

            if (lengthSquared(nextDirection) < 1e-12f)
                break;

            throughput = multiply(throughput, surfaceColor);

            if (lengthSquared(throughput) < 1e-6f)
                break;

            ray.origin = add(hitPosition, scale(nextDirection, RAY_EPSILON));
            ray.direction = nextDirection;
            continue;
        }

        const Vec3 viewDirection = normalize(negate(ray.direction));
        const float phongCoefficient =
            material.ambientReflection + material.diffuseReflection + material.specularReflection;

        if (material.type == MATERIAL_EMISSIVE)
            accumulated = add(accumulated, multiply(throughput, material.emissionColor));

        for (int lightIndex = 0; lightIndex < scene.lightCount; ++lightIndex)
        {
            const Light &light = scene.lights[lightIndex];
            const LightSample sample = sampleLight(scene, light, hitPosition, normal, shadingNormal, state);

            if (!sample.valid)
                continue;

            Vec3 contribution;

            if (light.type == LIGHT_AMBIENT)
                contribution = scale(sample.radiance, phongCoefficient);
            else
            {
                const float diffuseFactor = fmaxf(dot(shadingNormal, sample.direction), 0.0f);
                const Vec3 halfDirection = normalize(add(sample.direction, viewDirection));
                const float specularFactor =
                    powf(fmaxf(dot(shadingNormal, halfDirection), 0.0f), blinnShininess(material.roughness));

                contribution = add(scale(sample.radiance, phongCoefficient * diffuseFactor),
                                   scale(sample.radiance, material.specularReflection * specularFactor));
            }

            const Vec3 phongColor = multiply(multiply(contribution, surfaceColor), throughput);

            accumulated = add(accumulated, phongColor);
        }

        if (material.type == MATERIAL_EMISSIVE || material.type == MATERIAL_FLAT_COLOR)
            break;

        if (material.type == MATERIAL_DIFFUSE)
        {
            throughput = multiply(throughput, surfaceColor);

            if (lengthSquared(throughput) < 1e-6f)
                break;
        }

        Vec3 nextDirection = makeVec3(0.0f, 0.0f, 0.0f);

        if (material.type == MATERIAL_DIFFUSE)
            nextDirection = randomCosineHemisphere(shadingNormal, state);
        else if (material.type == MATERIAL_METALLIC)
        {
            Vec3 weight;

            if (!sampleMetallic(viewDirection, shadingNormal, surfaceColor, material.roughness, state,
                                nextDirection, weight))
                break;

            throughput = multiply(throughput, weight);

            if (lengthSquared(throughput) < 1e-6f)
                break;
        }
        else
            break;

        if (!isfinite(lengthSquared(nextDirection)) || lengthSquared(nextDirection) < 1e-12f ||
            dot(nextDirection, normal) <= 0.0f)
            break;

        ray.origin = add(hitPosition, scale(normal, RAY_EPSILON));
        ray.direction = nextDirection;
    }

    return accumulated;
}

__device__ inline uchar4 postProcessPixel(const SceneView &scene, const float4 &sum, int renderedSamples)
{
    const float inverseSamples = 1.0f / static_cast<float>(renderedSamples);
    const float red = postProcessChannel(sum.x * inverseSamples, scene.postProcess);
    const float green = postProcessChannel(sum.y * inverseSamples, scene.postProcess);
    const float blue = postProcessChannel(sum.z * inverseSamples, scene.postProcess);
    uchar4 pixel;

    pixel.x = static_cast<unsigned char>(red * 255.0f + 0.5f);
    pixel.y = static_cast<unsigned char>(green * 255.0f + 0.5f);
    pixel.z = static_cast<unsigned char>(blue * 255.0f + 0.5f);
    pixel.w = 255;
    return pixel;
}

__global__ void renderKernel(SceneView scene, int sampleStart, int sampleCount, float4 *accumulation, uchar4 *output)
{
    const int x = blockIdx.x * blockDim.x + threadIdx.x;
    const int y = blockIdx.y * blockDim.y + threadIdx.y;

    if (x >= scene.camera.width || y >= scene.camera.height)
        return;

    const std::size_t index = static_cast<std::size_t>(x) +
                              static_cast<std::size_t>(y) * static_cast<std::size_t>(scene.camera.width);
    float4 sum = accumulation[index];

    for (int sampleOffset = 0; sampleOffset < sampleCount; ++sampleOffset)
    {
        const Vec3 color = traceSample(scene, x, y, sampleStart + sampleOffset);

        sum.x += color.x;
        sum.y += color.y;
        sum.z += color.z;
    }

    accumulation[index] = sum;
    output[index] = postProcessPixel(scene, sum, sampleStart + sampleCount);
}

__global__ void postProcessKernel(SceneView scene, const float4 *accumulation, int renderedSamples, uchar4 *output)
{
    const int x = blockIdx.x * blockDim.x + threadIdx.x;
    const int y = blockIdx.y * blockDim.y + threadIdx.y;

    if (x >= scene.camera.width || y >= scene.camera.height)
        return;

    const std::size_t index = static_cast<std::size_t>(x) +
                              static_cast<std::size_t>(y) * static_cast<std::size_t>(scene.camera.width);

    output[index] = postProcessPixel(scene, accumulation[index], renderedSamples);
}
} // namespace

void launchRender(const SceneView &scene, int sampleStart, int sampleCount, float4 *accumulation, unsigned char *output,
                  cudaStream_t stream)
{
    if (sampleCount <= 0)
        return;

    const dim3 block(16, 16);
    const dim3 grid(static_cast<unsigned int>((scene.camera.width + 15) / 16),
                    static_cast<unsigned int>((scene.camera.height + 15) / 16));

    renderKernel<<<grid, block, 0, stream>>>(scene, sampleStart, sampleCount, accumulation,
                                             reinterpret_cast<uchar4 *>(output));
}

void launchPostProcess(const SceneView &scene, const float4 *accumulation, int renderedSamples, unsigned char *output,
                       cudaStream_t stream)
{
    if (renderedSamples <= 0)
        return;

    const dim3 block(16, 16);
    const dim3 grid(static_cast<unsigned int>((scene.camera.width + 15) / 16),
                    static_cast<unsigned int>((scene.camera.height + 15) / 16));

    postProcessKernel<<<grid, block, 0, stream>>>(scene, accumulation, renderedSamples,
                                                  reinterpret_cast<uchar4 *>(output));
}

} // namespace Cuda
} // namespace RayTracer
