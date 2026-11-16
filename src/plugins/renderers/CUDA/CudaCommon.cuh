/*
** EPITECH PROJECT, 2024
** raytracer
** File description:
** CudaCommon.cuh
*/

#ifndef CUDA_COMMON_CUH_
#define CUDA_COMMON_CUH_

#include "plugins/renderers/CUDA/CudaScene.hpp"
#include "render/SamplingMath.hpp"
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
    return clamp01Sampling(value);
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
        // Keep the draw order explicit: multiple randomFloat(state) calls in
        // one expression would have an unspecified evaluation order and are
        // unsequenced modifications of state.
        const float z = randomFloat(state) * 2.0f - 1.0f;
        const float y = randomFloat(state) * 2.0f - 1.0f;
        const float x = randomFloat(state) * 2.0f - 1.0f;
        const Vec3 value = makeVec3(x, y, z);
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
    return smithGGXSampling(ndot, alpha);
}

__device__ inline Vec3 fresnelSchlick(const Vec3 &f0, float cosTheta)
{
    const float factor = schlickFactorSampling(cosTheta);
    const Vec3 white = makeVec3(1.0f, 1.0f, 1.0f);

    return add(f0, scale(sub(white, f0), factor));
}

__device__ inline Vec3 stableTangent(const Vec3 &normal)
{
    const Vec3 axis = fabsf(normal.y) < 0.999f ? makeVec3(0.0f, 1.0f, 0.0f) : makeVec3(1.0f, 0.0f, 0.0f);
    Vec3 tangent = cross(axis, normal);
    const float length = lengthSquared(tangent);

    if (!isfinite(length) || length < 1e-12f)
        return makeVec3(0.0f, 0.0f, 0.0f);
    return normalize(tangent);
}

__device__ inline Vec3 orthonormalizeTangent(const Vec3 &tangent, const Vec3 &normal)
{
    Vec3 result = sub(tangent, scale(normal, dot(normal, tangent)));
    const float length = lengthSquared(result);

    if (!isfinite(length) || length < 1e-12f)
        return stableTangent(normal);
    return normalize(result);
}

__device__ inline Vec3 transformDirection(const TransformData &transform, const Vec3 &vector)
{
    return makeVec3(transform.linear[0] * vector.x + transform.linear[1] * vector.y + transform.linear[2] * vector.z,
                    transform.linear[3] * vector.x + transform.linear[4] * vector.y + transform.linear[5] * vector.z,
                    transform.linear[6] * vector.x + transform.linear[7] * vector.y + transform.linear[8] * vector.z);
}

__device__ inline bool atmosphereActive(const Atmosphere &atmosphere)
{
    return atmosphere.type != ATMOSPHERE_NONE && atmosphere.density > 0.0f;
}

__device__ inline float atmosphereOpticalDepth(const Atmosphere &atmosphere, const Vec3 &origin,
                                               const Vec3 &direction, float distance)
{
    if (!(distance > 0.0f))
        return 0.0f;

    if (atmosphere.type == ATMOSPHERE_HOMOGENEOUS)
        return atmosphere.density * distance;

    const float falloff = atmosphere.heightFalloff;
    const float base = atmosphere.density * expf(-falloff * (origin.y - atmosphere.groundY));
    const float vertical = falloff * direction.y;

    if (!isfinite(base) || base < 0.0f)
        return INFINITY;
    if (fabsf(vertical) < 1e-5f)
        return base * distance;

    const float depth = -expm1f(-vertical * distance) / vertical;

    if (isinf(depth))
        return depth > 0.0f ? INFINITY : 0.0f;
    return isfinite(depth) && depth > 0.0f ? base * depth : 0.0f;
}

__device__ inline float atmosphereTransmittance(const Atmosphere &atmosphere, const Vec3 &origin,
                                                const Vec3 &direction, float distance)
{
    if (!atmosphereActive(atmosphere))
        return 1.0f;

    const float depth =
        atmosphereOpticalDepth(atmosphere, origin, direction, fminf(distance, atmosphere.maxDistance));

    if (!(depth > 0.0f))
        return 1.0f;
    if (!isfinite(depth))
        return 0.0f;
    return expf(-depth);
}

__device__ inline bool sampleAtmosphereDistance(const Atmosphere &atmosphere, const Vec3 &origin,
                                                const Vec3 &direction, float sample, float &distance)
{
    if (!atmosphereActive(atmosphere))
        return false;

    const float clamped = fminf(fmaxf(sample, 0.0f), 0.99999994f);
    const float target = -logf(1.0f - clamped);

    if (atmosphere.type == ATMOSPHERE_HOMOGENEOUS)
    {
        distance = target / atmosphere.density;
        return isfinite(distance) && distance >= 0.0f;
    }

    const float falloff = atmosphere.heightFalloff;
    const float base = atmosphere.density * expf(-falloff * (origin.y - atmosphere.groundY));
    const float vertical = falloff * direction.y;

    if (!(base > 0.0f) || !isfinite(base))
        return false;
    if (fabsf(vertical) < 1e-5f)
    {
        distance = target / base;
        return isfinite(distance) && distance >= 0.0f;
    }

    if (vertical > 0.0f && target >= base / vertical)
        return false;

    const float remaining = 1.0f - target * vertical / base;

    if (!(remaining > 0.0f))
        return false;

    distance = -logf(remaining) / vertical;
    return isfinite(distance) && distance >= 0.0f;
}

__device__ inline float henyeyGreensteinPhase(float cosine, float anisotropy)
{
    return henyeyGreensteinPhaseSampling(cosine, anisotropy);
}

__device__ inline Vec3 sampleHenyeyGreenstein(const Vec3 &direction, float anisotropy, float u1, float u2)
{
    const float g = fminf(fmaxf(anisotropy, -0.999f), 0.999f);
    const float uniform = fminf(fmaxf(u1, 0.0f), 0.99999994f);
    float cosine;

    if (fabsf(g) < 1e-3f)
        cosine = 1.0f - 2.0f * uniform;
    else
    {
        const float root = (1.0f - g * g) / (1.0f + g - 2.0f * g * uniform);

        cosine = (1.0f + g * g - root * root) / (2.0f * g);
    }

    cosine = fminf(fmaxf(cosine, -1.0f), 1.0f);

    const float sine = sqrtf(fmaxf(0.0f, 1.0f - cosine * cosine));
    const float phi = 2.0f * PI_F * fminf(fmaxf(u2, 0.0f), 0.99999994f);
    Vec3 tangent = stableTangent(direction);

    if (lengthSquared(tangent) < 1e-12f)
        tangent = makeVec3(1.0f, 0.0f, 0.0f);

    const Vec3 bitangent = cross(direction, tangent);
    const Vec3 scattered =
        add(add(scale(tangent, cosf(phi) * sine), scale(bitangent, sinf(phi) * sine)), scale(direction, cosine));

    if (lengthSquared(scattered) < 1e-12f)
        return direction;
    return normalize(scattered);
}

__device__ inline float blinnShininess(float roughness)
{
    return blinnShininessSampling(roughness);
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

} // namespace
} // namespace Cuda
} // namespace RayTracer

#endif // CUDA_COMMON_CUH_
