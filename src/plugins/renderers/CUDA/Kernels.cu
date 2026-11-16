/*
** EPITECH PROJECT, 2024
** raytracer
** File description:
** CUDA kernels
*/

#include "plugins/renderers/CUDA/CudaKernels.hpp"
#include "CudaCommon.cuh"
#include "CudaIntersections.cuh"
#include "render/DielectricStack.hpp"
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

__device__ LightSample sampleLight(const SceneView &scene, const Light &light, const Vec3 &hitPosition,
                                   const Vec3 &hitNormal, const Vec3 &shadingNormal, unsigned int &state,
                                   bool medium = false)
{
    LightSample sample{makeVec3(0.0f, 0.0f, 0.0f), makeVec3(0.0f, 0.0f, 0.0f), INFINITY, false};

    if (light.type == LIGHT_AMBIENT)
    {
        sample.radiance = scale(light.color, light.intensity);
        sample.distance = scene.atmosphere.maxDistance;
        sample.valid = true;
        return sample;
    }

    if (light.type == LIGHT_DIRECTIONAL)
    {
        sample.direction = normalize(light.direction);

        if (!medium && dot(sample.direction, shadingNormal) <= 0.0f)
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

        if (!medium && dot(sample.direction, shadingNormal) <= 0.0f)
            return sample;

        const float denominator = light.attenuationConstant + light.attenuationLinear * distance +
                                  light.attenuationQuadratic * distance * distance;
        const float attenuation = denominator > 0.0f ? 1.0f / denominator : 0.0f;

        if (occluded(scene, Ray{add(hitPosition, scale(hitNormal, RAY_EPSILON)), sample.direction}, distance))
            return sample;

        sample.radiance = scale(light.color, light.intensity * attenuation);
        sample.distance = distance;
        sample.valid = true;
        return sample;
    }

    if (light.type == LIGHT_AREA_SPHERE)
    {
        const Vec3 toCenter = sub(light.position, hitPosition);
        const float centerDistance = length(toCenter);

        if (!isfinite(centerDistance) || centerDistance <= light.radius)
            return sample;

        const float sinThetaMax = fminf(1.0f, light.radius / centerDistance);
        const float cosThetaMax = sqrtf(fmaxf(0.0f, 1.0f - sinThetaMax * sinThetaMax));
        const float u = randomFloat(state);
        const float v = randomFloat(state);
        const float cosTheta = 1.0f - u * (1.0f - cosThetaMax);
        const float sinTheta = sqrtf(fmaxf(0.0f, 1.0f - cosTheta * cosTheta));
        const float phi = 2.0f * PI_F * v;
        const Vec3 w = scale(toCenter, 1.0f / centerDistance);
        const Vec3 up = fabsf(w.y) < 0.999f ? makeVec3(0.0f, 1.0f, 0.0f) : makeVec3(1.0f, 0.0f, 0.0f);
        const Vec3 tangent = normalize(cross(up, w));
        const Vec3 bitangent = cross(w, tangent);

        sample.direction = normalize(add(add(scale(tangent, cosf(phi) * sinTheta),
                                             scale(bitangent, sinf(phi) * sinTheta)),
                                         scale(w, cosTheta)));

        if (!medium && dot(sample.direction, shadingNormal) <= 0.0f)
            return sample;

        const Vec3 originToCenter = sub(hitPosition, light.position);
        const float b = dot(originToCenter, sample.direction);
        const float c = dot(originToCenter, originToCenter) - light.radius * light.radius;
        const float discriminant = b * b - c;
        float shadowDistance = centerDistance - light.radius;

        if (discriminant >= 0.0f)
        {
            const float entryDistance = -b - sqrtf(discriminant);

            if (entryDistance > RAY_EPSILON)
                shadowDistance = entryDistance;
        }

        const float denominator = light.attenuationConstant + light.attenuationLinear * centerDistance +
                                  light.attenuationQuadratic * centerDistance * centerDistance;
        const float attenuation = denominator > 0.0f ? 1.0f / denominator : 0.0f;
        const float solidAngle = 2.0f * PI_F * (1.0f - cosThetaMax);

        if (occluded(scene, Ray{add(hitPosition, scale(hitNormal, RAY_EPSILON)), sample.direction},
                     shadowDistance))
            return sample;

        sample.radiance = scale(light.color, light.intensity * attenuation * solidAngle);
        sample.distance = shadowDistance;
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

__device__ inline float environmentLuminance(const Vec3 &color)
{
    return 0.2126f * color.x + 0.7152f * color.y + 0.0722f * color.z;
}

__device__ inline int sampleCdfIndex(const float *cdf, int count, float value)
{
    int low = 0;
    int high = count;

    while (low < high)
    {
        const int middle = (low + high) / 2;

        if (cdf[middle] < value)
            low = middle + 1;
        else
            high = middle;
    }

    return low < count ? low : count - 1;
}

__device__ inline bool environmentTexelLuminance(const SceneView &scene, int x, int y, float &luminance)
{
    const int textureIndex = scene.background.environmentTextureIndex;

    if (textureIndex < 0 || textureIndex >= scene.textureCount || scene.textures == nullptr || scene.texels == nullptr)
        return false;

    const TextureRange &range = scene.textures[textureIndex];

    if (x < 0 || x >= range.width || y < 0 || y >= range.height)
        return false;

    const int index = range.start + y * range.width + x;

    if (index < 0 || index >= scene.texelCount)
        return false;

    luminance = fmaxf(0.0f, environmentLuminance(scene.texels[index]));
    return true;
}

__device__ inline float powerHeuristicPdf(float first, float second)
{
    return powerHeuristicSampling(first, second);
}

__device__ inline Vec3 environmentDirection(float u, float v)
{
    const float theta = PI_F * v;
    const float phi = 2.0f * PI_F * (u - 0.5f);
    const float sinTheta = sinf(theta);

    return makeVec3(sinTheta * cosf(phi), cosf(theta), sinTheta * sinf(phi));
}

__device__ bool sampleEnvironment(const SceneView &scene, unsigned int &state, Vec3 &direction, Vec3 &radiance, float &pdf)
{
    const Background &background = scene.background;

    if (background.enabled == 0 || background.environmentConditionalCdf == nullptr ||
        background.environmentMarginalCdf == nullptr || background.environmentWidth <= 0 ||
        background.environmentHeight <= 0 || background.environmentTotal <= 0.0f)
        return false;

    const int y = sampleCdfIndex(background.environmentMarginalCdf, background.environmentHeight, randomFloat(state));

    if (y < 0)
        return false;

    const float *row = background.environmentConditionalCdf + static_cast<std::size_t>(y) * background.environmentWidth;
    const int x = sampleCdfIndex(row, background.environmentWidth, randomFloat(state));

    if (x < 0)
        return false;

    float texelLuminance = 0.0f;

    if (!environmentTexelLuminance(scene, x, y, texelLuminance) || texelLuminance <= 0.0f)
        return false;

    const float u = (static_cast<float>(x) + 0.5f) / static_cast<float>(background.environmentWidth);
    const float v = (static_cast<float>(y) + 0.5f) / static_cast<float>(background.environmentHeight);

    direction = environmentDirection(u, v);
    radiance = sampleTextureClamped(scene, background.environmentTextureIndex, u, v);
    pdf = texelLuminance * static_cast<float>(background.environmentWidth) *
          static_cast<float>(background.environmentHeight) / (background.environmentTotal * 2.0f * PI_F * PI_F);
    return true;
}

__device__ float environmentPdf(const SceneView &scene, const Vec3 &direction)
{
    const Background &background = scene.background;

    if (background.enabled == 0 || background.environmentConditionalCdf == nullptr || background.environmentWidth <= 0 ||
        background.environmentHeight <= 0 || background.environmentTotal <= 0.0f)
        return 0.0f;

    const Vec3 unit = normalize(direction);

    if (lengthSquared(unit) < 1e-12f)
        return 0.0f;

    const float u = 0.5f + atan2f(unit.z, unit.x) / (2.0f * PI_F);
    const float v = acosf(fmaxf(-1.0f, fminf(1.0f, unit.y))) / PI_F;
    const int x = max(0, min(background.environmentWidth - 1, static_cast<int>(floorf(u * background.environmentWidth))));
    const int y = max(0, min(background.environmentHeight - 1, static_cast<int>(floorf(v * background.environmentHeight))));
    float texelLuminance = 0.0f;

    if (!environmentTexelLuminance(scene, x, y, texelLuminance) || texelLuminance <= 0.0f)
        return 0.0f;

    return texelLuminance * static_cast<float>(background.environmentWidth) *
           static_cast<float>(background.environmentHeight) / (background.environmentTotal * 2.0f * PI_F * PI_F);
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
    float backgroundWeight = 1.0f;
    DielectricStack dielectricStack;

    dielectricReset(dielectricStack);

    for (int bounce = 0; bounce < MAX_BOUNCES; ++bounce)
    {
        float distance = 0.0f;
        Vec3 normal = makeVec3(0.0f, 0.0f, 0.0f);
        Vec3 shadingNormal = makeVec3(0.0f, 0.0f, 0.0f);
        Vec3 shadingTangent = makeVec3(0.0f, 0.0f, 0.0f);
        float u = 0.0f;
        float v = 0.0f;
        int materialIndex = -1;
        int primitiveIndex = -1;

        const bool hitSurface = intersectScene(scene, ray, distance, normal, shadingNormal, shadingTangent, u, v,
                                               materialIndex, primitiveIndex);
        bool mediumScattered = false;

        if (atmosphereActive(scene.atmosphere))
        {
            const Vec3 direction = ray.direction;
            const float directionLength = length(direction);

            if (directionLength > 0.0f)
            {
                const Vec3 unitDirection = scale(direction, 1.0f / directionLength);
                float segmentDistance = scene.atmosphere.maxDistance;

                if (hitSurface)
                    segmentDistance = fminf(segmentDistance, distance * directionLength);

                float scatterDistance = 0.0f;
                const float distanceSample = randomFloat(state);

                if (sampleAtmosphereDistance(scene.atmosphere, ray.origin, unitDirection, distanceSample,
                                             scatterDistance) &&
                    scatterDistance < segmentDistance)
                {
                    const Vec3 scatterPosition = rayAt(ray, scatterDistance / directionLength);

                    for (int lightIndex = 0; lightIndex < scene.lightCount; ++lightIndex)
                    {
                        const Light &light = scene.lights[lightIndex];
                        const LightSample lightSample =
                            sampleLight(scene, light, scatterPosition, makeVec3(0.0f, 0.0f, 0.0f),
                                        makeVec3(0.0f, 0.0f, 0.0f), state, true);

                        if (!lightSample.valid)
                            continue;

                        const bool ambient = light.type == LIGHT_AMBIENT;
                        const float lightDistance =
                            ambient ? scene.atmosphere.maxDistance
                                    : fminf(lightSample.distance, scene.atmosphere.maxDistance);
                        const Vec3 lightDirection = ambient ? unitDirection : lightSample.direction;
                        const float transmittance = atmosphereTransmittance(scene.atmosphere, scatterPosition,
                                                                           lightDirection, lightDistance);
                        const Vec3 inScattering =
                            multiply(throughput, multiply(scene.atmosphere.color, lightSample.radiance));

                        if (ambient)
                        {
                            accumulated = add(accumulated, scale(inScattering, transmittance));
                            continue;
                        }

                        const float cosine = fminf(fmaxf(dot(unitDirection, lightSample.direction), -1.0f), 1.0f);
                        const float phase = henyeyGreensteinPhase(cosine, scene.atmosphere.anisotropy);

                        accumulated = add(accumulated, scale(inScattering, phase * transmittance));
                    }

                    const float directionU = randomFloat(state);
                    const float directionV = randomFloat(state);
                    const Vec3 nextDirection = sampleHenyeyGreenstein(unitDirection, scene.atmosphere.anisotropy,
                                                                      directionU, directionV);

                    throughput = multiply(throughput, scene.atmosphere.color);
                    ray.origin = add(scatterPosition, scale(nextDirection, RAY_EPSILON));
                    ray.direction = nextDirection;
                    backgroundWeight = 1.0f;
                    mediumScattered = true;
                }
                else
                {
                    const float transmittance =
                        atmosphereTransmittance(scene.atmosphere, ray.origin, unitDirection, segmentDistance);

                    throughput = scale(throughput, transmittance);
                }
            }
        }

        if (mediumScattered)
            continue;

        if (!hitSurface)
        {
            if (scene.background.enabled != 0)
                accumulated = add(accumulated,
                                  scale(multiply(throughput, skyColor(scene, ray.direction)), backgroundWeight));
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
        float roughness = material.roughness;
        Vec3 emissionColor = material.emissionColor;

        if (material.roughnessTextureIndex >= 0)
            roughness = clamp01(roughness * sampleTexture(scene, material.roughnessTextureIndex, u, v).x);
        if (material.emissionTextureIndex >= 0)
            emissionColor = multiply(emissionColor, sampleTexture(scene, material.emissionTextureIndex, u, v));
        if (material.normalTextureIndex >= 0)
        {
            const Vec3 sample = sampleTexture(scene, material.normalTextureIndex, u, v);
            const Vec3 normalUnit = normalize(shadingNormal);
            const Vec3 tangent = orthonormalizeTangent(shadingTangent, normalUnit);

            if (lengthSquared(tangent) >= 1e-12f)
            {
                const Vec3 bitangent = cross(normalUnit, tangent);
                const Vec3 mapped = makeVec3(2.0f * sample.x - 1.0f, 2.0f * sample.y - 1.0f,
                                             2.0f * sample.z - 1.0f);
                Vec3 perturbed =
                    add(add(scale(tangent, mapped.x), scale(bitangent, mapped.y)), scale(normalUnit, mapped.z));

                if (lengthSquared(perturbed) >= 1e-12f)
                {
                    perturbed = normalize(perturbed);
                    if (dot(perturbed, ray.direction) > 0.0f)
                        perturbed = negate(perturbed);
                    shadingNormal = perturbed;
                }
            }
        }

        if (material.type == MATERIAL_DIELECTRIC)
        {
            // Camera rays are not normalized. Fresnel and Snell's law require
            // a unit incident vector.
            const Vec3 incident = normalize(ray.direction);

            if (lengthSquared(incident) < 1e-12f)
                break;

            const float cosi = clamp01(-dot(incident, normal));
            const int stackPosition = dielectricFind(dielectricStack, primitiveIndex);
            const bool exiting = stackPosition >= 0;
            const float etaI = exiting ? material.ior : dielectricSurroundingIor(dielectricStack);
            const float etaT = exiting ? dielectricExteriorIor(dielectricStack, stackPosition) : material.ior;
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

                if (exiting)
                    dielectricRemove(dielectricStack, primitiveIndex);
                else if (!dielectricPush(dielectricStack, primitiveIndex, material.ior))
                    break;
            }

            if (lengthSquared(nextDirection) < 1e-12f)
                break;

            throughput = multiply(throughput, surfaceColor);

            if (lengthSquared(throughput) < 1e-6f)
                break;

            ray.origin = add(hitPosition, scale(nextDirection, RAY_EPSILON));
            ray.direction = nextDirection;
            backgroundWeight = 1.0f;
            continue;
        }

        const Vec3 viewDirection = normalize(negate(ray.direction));
        const float phongCoefficient =
            material.ambientReflection + material.diffuseReflection + material.specularReflection;

        if (lengthSquared(emissionColor) > 0.0f)
            accumulated = add(accumulated, multiply(throughput, emissionColor));

        for (int lightIndex = 0; lightIndex < scene.lightCount; ++lightIndex)
        {
            const Light &light = scene.lights[lightIndex];
            const LightSample sample = sampleLight(scene, light, hitPosition, normal, shadingNormal, state);

            if (!sample.valid)
                continue;

            const bool ambientLight = light.type == LIGHT_AMBIENT;
            const float lightDistance = ambientLight ? scene.atmosphere.maxDistance
                                                     : fminf(sample.distance, scene.atmosphere.maxDistance);
            const Vec3 transmittanceDirection =
                ambientLight ? makeVec3(0.0f, 1.0f, 0.0f) : sample.direction;
            const float lightTransmittance = atmosphereTransmittance(scene.atmosphere, hitPosition,
                                                                    transmittanceDirection, lightDistance);

            Vec3 contribution;

            if (light.type == LIGHT_AMBIENT)
                contribution = scale(sample.radiance, phongCoefficient);
            else
            {
                const float diffuseFactor = fmaxf(dot(shadingNormal, sample.direction), 0.0f);
                const Vec3 halfDirection = normalize(add(sample.direction, viewDirection));
                const float specularFactor =
                    powf(fmaxf(dot(shadingNormal, halfDirection), 0.0f), blinnShininess(roughness));

                contribution = add(scale(sample.radiance, phongCoefficient * diffuseFactor),
                                   scale(sample.radiance, material.specularReflection * specularFactor));
            }

            const Vec3 phongColor = multiply(multiply(contribution, surfaceColor), throughput);

            accumulated = add(accumulated, scale(phongColor, lightTransmittance));
        }

        if (material.type == MATERIAL_EMISSIVE || material.type == MATERIAL_FLAT_COLOR)
            break;

        if (material.type == MATERIAL_DIFFUSE)
        {
            throughput = multiply(throughput, surfaceColor);

            if (lengthSquared(throughput) < 1e-6f)
                break;
        }

        float nextBackgroundWeight = 1.0f;

        // Explicit environment lighting for diffuse surfaces, combined with
        // cosine-weighted BSDF sampling through MIS.
        if (material.type == MATERIAL_DIFFUSE && scene.background.enabled != 0)
        {
            Vec3 environmentDirection;
            Vec3 environmentRadiance;
            float environmentSamplePdf = 0.0f;

            if (sampleEnvironment(scene, state, environmentDirection, environmentRadiance, environmentSamplePdf) &&
                environmentSamplePdf > 0.0f)
            {
                const float environmentCosine = fmaxf(dot(shadingNormal, environmentDirection), 0.0f);

                if (environmentCosine > 0.0f)
                {
                    const float bsdfPdfAtEnvironment = environmentCosine / PI_F;
                    const float environmentWeight =
                        powerHeuristicPdf(environmentSamplePdf, bsdfPdfAtEnvironment);
                    const float contributionScale =
                        environmentCosine / (PI_F * environmentSamplePdf) * environmentWeight;

                    const float environmentTransmittance = atmosphereTransmittance(
                        scene.atmosphere, hitPosition, environmentDirection, scene.atmosphere.maxDistance);

                    accumulated = add(accumulated, scale(multiply(throughput, environmentRadiance),
                                                         contributionScale * environmentTransmittance));
                }
            }
        }

        Vec3 nextDirection = makeVec3(0.0f, 0.0f, 0.0f);

        if (material.type == MATERIAL_DIFFUSE)
            nextDirection = randomCosineHemisphere(shadingNormal, state);
        else if (material.type == MATERIAL_METALLIC)
        {
            Vec3 weight;

            if (!sampleMetallic(viewDirection, shadingNormal, surfaceColor, roughness, state, nextDirection, weight))
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

        if (material.type == MATERIAL_DIFFUSE)
        {
            const float bsdfCosine = fmaxf(dot(shadingNormal, nextDirection), 0.0f);
            const float bsdfPdf = bsdfCosine / PI_F;
            const float environmentPdfValue = environmentPdf(scene, nextDirection);

            nextBackgroundWeight = powerHeuristicPdf(bsdfPdf, environmentPdfValue);
        }

        backgroundWeight = nextBackgroundWeight;
        ray.origin = add(hitPosition, scale(normal, RAY_EPSILON));
        ray.direction = nextDirection;
    }

    return accumulated;
}

__device__ inline uchar4 postProcessPixel(const SceneView &scene, const float4 &sum, int renderedSamples,
                                          const float4 *bloom, std::size_t index)
{
    const float inverseSamples = 1.0f / static_cast<float>(renderedSamples);
    float bloomRed = 0.0f;
    float bloomGreen = 0.0f;
    float bloomBlue = 0.0f;

    if (bloom != nullptr && scene.postProcess.bloom != 0)
    {
        const float4 value = bloom[index];

        bloomRed = value.x * scene.postProcess.bloomIntensity;
        bloomGreen = value.y * scene.postProcess.bloomIntensity;
        bloomBlue = value.z * scene.postProcess.bloomIntensity;
    }

    const float red = postProcessChannel(sum.x * inverseSamples + bloomRed, scene.postProcess);
    const float green = postProcessChannel(sum.y * inverseSamples + bloomGreen, scene.postProcess);
    const float blue = postProcessChannel(sum.z * inverseSamples + bloomBlue, scene.postProcess);
    uchar4 pixel;

    pixel.x = static_cast<unsigned char>(red * 255.0f + 0.5f);
    pixel.y = static_cast<unsigned char>(green * 255.0f + 0.5f);
    pixel.z = static_cast<unsigned char>(blue * 255.0f + 0.5f);
    pixel.w = 255;
    return pixel;
}

__global__ void bloomBrightPassKernel(SceneView scene, const float4 *accumulation, int renderedSamples, float4 *bloom)
{
    const int x = blockIdx.x * blockDim.x + threadIdx.x;
    const int y = blockIdx.y * blockDim.y + threadIdx.y;

    if (x >= scene.camera.width || y >= scene.camera.height)
        return;

    const std::size_t index = static_cast<std::size_t>(x) +
                              static_cast<std::size_t>(y) * static_cast<std::size_t>(scene.camera.width);
    const float inverseSamples = 1.0f / static_cast<float>(renderedSamples);
    const float4 sum = accumulation[index];
    const Vec3 color = makeVec3(sum.x * inverseSamples, sum.y * inverseSamples, sum.z * inverseSamples);
    const float luminance = 0.2126f * color.x + 0.7152f * color.y + 0.0722f * color.z;

    bloom[index] = luminance > scene.postProcess.bloomThreshold ? make_float4(color.x, color.y, color.z, 0.0f)
                                                                : make_float4(0.0f, 0.0f, 0.0f, 0.0f);
}

__global__ void bloomBlurHorizontalKernel(SceneView scene, const float4 *input, float4 *output)
{
    const int x = blockIdx.x * blockDim.x + threadIdx.x;
    const int y = blockIdx.y * blockDim.y + threadIdx.y;

    if (x >= scene.camera.width || y >= scene.camera.height)
        return;

    const int radius = scene.postProcess.bloomRadius;
    float red = 0.0f;
    float green = 0.0f;
    float blue = 0.0f;

    for (int offset = -radius; offset <= radius; ++offset)
    {
        const int sampleX = max(0, min(scene.camera.width - 1, x + offset));
        const int index = y * scene.camera.width + sampleX;
        const float4 value = input[index];
        const float weight = scene.postProcess.bloomWeights[offset + BLOOM_MAX_RADIUS];

        red += value.x * weight;
        green += value.y * weight;
        blue += value.z * weight;
    }

    const std::size_t index = static_cast<std::size_t>(x) +
                              static_cast<std::size_t>(y) * static_cast<std::size_t>(scene.camera.width);

    output[index] = make_float4(red, green, blue, 0.0f);
}

__global__ void bloomBlurVerticalKernel(SceneView scene, const float4 *input, float4 *output)
{
    const int x = blockIdx.x * blockDim.x + threadIdx.x;
    const int y = blockIdx.y * blockDim.y + threadIdx.y;

    if (x >= scene.camera.width || y >= scene.camera.height)
        return;

    const int radius = scene.postProcess.bloomRadius;
    float red = 0.0f;
    float green = 0.0f;
    float blue = 0.0f;

    for (int offset = -radius; offset <= radius; ++offset)
    {
        const int sampleY = max(0, min(scene.camera.height - 1, y + offset));
        const int index = sampleY * scene.camera.width + x;
        const float4 value = input[index];
        const float weight = scene.postProcess.bloomWeights[offset + BLOOM_MAX_RADIUS];

        red += value.x * weight;
        green += value.y * weight;
        blue += value.z * weight;
    }

    const std::size_t index = static_cast<std::size_t>(x) +
                              static_cast<std::size_t>(y) * static_cast<std::size_t>(scene.camera.width);

    output[index] = make_float4(red, green, blue, 0.0f);
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
    output[index] = postProcessPixel(scene, sum, sampleStart + sampleCount, nullptr, index);
}

__global__ void postProcessKernel(SceneView scene, const float4 *accumulation, const float4 *bloom, int renderedSamples,
                                  uchar4 *output)
{
    const int x = blockIdx.x * blockDim.x + threadIdx.x;
    const int y = blockIdx.y * blockDim.y + threadIdx.y;

    if (x >= scene.camera.width || y >= scene.camera.height)
        return;

    const std::size_t index = static_cast<std::size_t>(x) +
                              static_cast<std::size_t>(y) * static_cast<std::size_t>(scene.camera.width);

    output[index] = postProcessPixel(scene, accumulation[index], renderedSamples, bloom, index);
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

namespace
{
__global__ void accumulateShutterKernel(const float4 *source, float4 *destination, int count)
{
    const int index = static_cast<int>(blockIdx.x * blockDim.x + threadIdx.x);

    if (index >= count)
        return;

    const float4 value = source[index];
    const float4 accumulated = destination[index];

    destination[index] = make_float4(accumulated.x + value.x, accumulated.y + value.y, accumulated.z + value.z,
                                     accumulated.w + value.w);
}
} // namespace

void launchAccumulateShutter(const float4 *source, float4 *destination, int count, cudaStream_t stream)
{
    if (count <= 0)
        return;

    constexpr int blockSize = 256;
    const int gridSize = (count + blockSize - 1) / blockSize;

    accumulateShutterKernel<<<gridSize, blockSize, 0, stream>>>(source, destination, count);
}

void launchPostProcess(const SceneView &scene, const float4 *accumulation, int renderedSamples, float4 *bloomA,
                       float4 *bloomB, unsigned char *output, cudaStream_t stream)
{
    if (renderedSamples <= 0)
        return;

    const dim3 block(16, 16);
    const dim3 grid(static_cast<unsigned int>((scene.camera.width + 15) / 16),
                    static_cast<unsigned int>((scene.camera.height + 15) / 16));
    const float4 *bloom = nullptr;

    if (scene.postProcess.bloom != 0 && bloomA != nullptr && bloomB != nullptr)
    {
        bloomBrightPassKernel<<<grid, block, 0, stream>>>(scene, accumulation, renderedSamples, bloomA);
        bloomBlurHorizontalKernel<<<grid, block, 0, stream>>>(scene, bloomA, bloomB);
        bloomBlurVerticalKernel<<<grid, block, 0, stream>>>(scene, bloomB, bloomA);
        bloom = bloomA;
    }

    postProcessKernel<<<grid, block, 0, stream>>>(scene, accumulation, bloom, renderedSamples,
                                                  reinterpret_cast<uchar4 *>(output));
}

} // namespace Cuda
} // namespace RayTracer
