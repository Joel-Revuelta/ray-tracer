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
constexpr int MAX_BOUNCES = 15;

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

struct Ray
{
    Vec3 origin;
    Vec3 direction;
};

__device__ inline Vec3 rayAt(const Ray &ray, float distance)
{
    return add(ray.origin, scale(ray.direction, distance));
}

__device__ bool intersectSphere(const Primitive &primitive, const Ray &ray, float &bestDistance, Vec3 &bestNormal,
                                int &bestIndex, int index)
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
    return true;
}

__device__ bool intersectPlane(const Primitive &primitive, const Ray &ray, float &bestDistance, Vec3 &bestNormal,
                               int &bestIndex, int index)
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
    return true;
}

__device__ bool intersectCylinder(const Primitive &primitive, const Ray &ray, float &bestDistance, Vec3 &bestNormal,
                                  int &bestIndex, int index)
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
                              int &bestIndex, int index)
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
    return true;
}

__device__ bool intersectPrimitive(const Primitive &primitive, const Ray &ray, float &bestDistance, Vec3 &bestNormal,
                                   int &bestIndex, int index)
{
    switch (primitive.type)
    {
    case PRIMITIVE_SPHERE:
        return intersectSphere(primitive, ray, bestDistance, bestNormal, bestIndex, index);
    case PRIMITIVE_PLANE:
        return intersectPlane(primitive, ray, bestDistance, bestNormal, bestIndex, index);
    case PRIMITIVE_CYLINDER:
        return intersectCylinder(primitive, ray, bestDistance, bestNormal, bestIndex, index);
    case PRIMITIVE_CONE:
        return intersectCone(primitive, ray, bestDistance, bestNormal, bestIndex, index);
    default:
        return false;
    }
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

__device__ bool intersectBVH(const SceneView &scene, const Ray &ray, float &distance, Vec3 &normal, int &index)
{
    int stack[64];
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

                if (intersectPrimitive(scene.primitives[primitiveIndex], ray, distance, normal, index, primitiveIndex))
                    hit = true;
            }
        }
        else
        {
            if (node.left >= 0)
                stack[stackSize++] = node.left;
            if (node.right >= 0)
                stack[stackSize++] = node.right;
        }
    }

    return hit;
}

__device__ bool intersectScene(const SceneView &scene, const Ray &ray, float &distance, Vec3 &normal, int &index)
{
    distance = INFINITY;
    normal = makeVec3(0.0f, 0.0f, 0.0f);
    index = -1;
    bool hit = false;

    if (scene.nodeCount > 0)
        hit = intersectBVH(scene, ray, distance, normal, index);
    else
    {
        for (int i = 0; i < scene.primitiveCount; ++i)
        {
            if (intersectPrimitive(scene.primitives[i], ray, distance, normal, index, i))
                hit = true;
        }
    }

    for (int i = 0; i < scene.unboundedCount; ++i)
    {
        const int primitiveIndex = scene.unboundedIndices[i];

        if (intersectPrimitive(scene.primitives[primitiveIndex], ray, distance, normal, index, primitiveIndex))
            hit = true;
    }

    return hit;
}

__device__ bool occluded(const SceneView &scene, const Ray &ray, float maxDistance)
{
    float distance = maxDistance;
    Vec3 normal = makeVec3(0.0f, 0.0f, 0.0f);
    int index = -1;

    if (scene.nodeCount > 0)
    {
        int stack[64];
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

                    if (intersectPrimitive(scene.primitives[primitiveIndex], ray, distance, normal, index,
                                           primitiveIndex))
                        return true;
                }
            }
            else
            {
                if (node.left >= 0)
                    stack[stackSize++] = node.left;
                if (node.right >= 0)
                    stack[stackSize++] = node.right;
            }
        }
    }
    else
    {
        for (int i = 0; i < scene.primitiveCount; ++i)
        {
            if (intersectPrimitive(scene.primitives[i], ray, distance, normal, index, i))
                return true;
        }
    }

    for (int i = 0; i < scene.unboundedCount; ++i)
    {
        const int primitiveIndex = scene.unboundedIndices[i];

        if (intersectPrimitive(scene.primitives[primitiveIndex], ray, distance, normal, index, primitiveIndex))
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
                                   const Vec3 &hitNormal)
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

        if (dot(sample.direction, hitNormal) <= 0.0f)
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

        if (dot(sample.direction, hitNormal) <= 0.0f)
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

__device__ Vec3 traceSample(const SceneView &scene, int x, int y, int sample)
{
    unsigned int state =
        pcgHash(static_cast<unsigned int>(x) + 0x9e3779b9u * static_cast<unsigned int>(y) +
                static_cast<unsigned int>(sample + 1));
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

    Ray ray{scene.camera.lookFrom, direction};
    Vec3 accumulated = makeVec3(0.0f, 0.0f, 0.0f);
    Vec3 throughput = makeVec3(1.0f, 1.0f, 1.0f);
    bool insideDielectric = false;

    for (int bounce = 0; bounce < MAX_BOUNCES; ++bounce)
    {
        float distance = 0.0f;
        Vec3 normal = makeVec3(0.0f, 0.0f, 0.0f);
        int primitiveIndex = -1;

        if (!intersectScene(scene, ray, distance, normal, primitiveIndex))
            break;

        const Primitive &primitive = scene.primitives[primitiveIndex];

        if (primitive.materialIndex >= static_cast<unsigned int>(scene.materialCount))
            break;

        const Material &material = scene.materials[primitive.materialIndex];
        const Vec3 hitPosition = rayAt(ray, distance);

        if (material.type == MATERIAL_DIELECTRIC)
        {
            const float cosi = clamp01(-dot(ray.direction, normal));
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
                nextDirection = normalize(reflect(ray.direction, normal));
            else
            {
                nextDirection =
                    normalize(add(scale(ray.direction, eta), scale(normal, eta * cosi - sqrtf(k))));
                insideDielectric = !insideDielectric;
            }

            if (lengthSquared(nextDirection) < 1e-12f)
                break;

            throughput = multiply(throughput, material.diffuseColor);

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
            const LightSample sample = sampleLight(scene, light, hitPosition, normal);

            if (!sample.valid)
                continue;

            Vec3 contribution;

            if (light.type == LIGHT_AMBIENT)
                contribution = scale(sample.radiance, phongCoefficient);
            else
            {
                const float diffuseFactor = fmaxf(dot(normal, sample.direction), 0.0f);
                const Vec3 halfDirection = normalize(add(sample.direction, viewDirection));
                const float specularFactor = powf(fmaxf(dot(normal, halfDirection), 0.0f), 64.0f);

                contribution = add(scale(sample.radiance, phongCoefficient * diffuseFactor),
                                   scale(sample.radiance, material.specularReflection * specularFactor));
            }

            const Vec3 phongColor = multiply(multiply(contribution, material.diffuseColor), throughput);

            accumulated = add(accumulated, phongColor);
        }

        if (material.type == MATERIAL_EMISSIVE || material.type == MATERIAL_FLAT_COLOR)
            break;

        if (material.type == MATERIAL_DIFFUSE || material.type == MATERIAL_METALLIC)
        {
            throughput = multiply(throughput, material.diffuseColor);

            if (lengthSquared(throughput) < 1e-6f)
                break;
        }

        Vec3 nextDirection = makeVec3(0.0f, 0.0f, 0.0f);

        if (material.type == MATERIAL_DIFFUSE)
            nextDirection = randomCosineHemisphere(normal, state);
        else if (material.type == MATERIAL_METALLIC)
            nextDirection = normalize(reflect(ray.direction, normal));
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

    const int renderedSamples = sampleStart + sampleCount;
    const float inverseSamples = 1.0f / static_cast<float>(renderedSamples);
    uchar4 pixel;

    const float red = clamp01(sqrtf(fmaxf(sum.x * inverseSamples, 0.0f)));
    const float green = clamp01(sqrtf(fmaxf(sum.y * inverseSamples, 0.0f)));
    const float blue = clamp01(sqrtf(fmaxf(sum.z * inverseSamples, 0.0f)));

    pixel.x = static_cast<unsigned char>(red * 255.0f + 0.5f);
    pixel.y = static_cast<unsigned char>(green * 255.0f + 0.5f);
    pixel.z = static_cast<unsigned char>(blue * 255.0f + 0.5f);
    pixel.w = 255;
    output[index] = pixel;
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

} // namespace Cuda
} // namespace RayTracer
