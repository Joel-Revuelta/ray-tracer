/*
** EPITECH PROJECT, 2023
** raytracer
** File description:
** RendererSampling.cpp
*/

#include "plugins/renderers/CPU/Renderer.hpp"
#include "render/AtmosphereSampling.hpp"
#include "render/Brdf.hpp"
#include "render/DielectricStack.hpp"
#include "render/Tangent.hpp"
#include "render/Texture.hpp"
#include "Utils.hpp"
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>
#include <stdexcept>

using namespace RayTracer;

namespace RayTracer
{
namespace
{
float powerHeuristicPdf(float first, float second)
{
    const float firstSquared = first * first;
    const float secondSquared = second * second;
    const float total = firstSquared + secondSquared;

    return total > 0.0f ? firstSquared / total : 0.0f;
}
} // namespace

/**
 * Computes the direction of the next ray to be traced based on the hit material.
 *
 * @param ray The current ray being traced.
 * @param payload The hit payload containing information about the intersection.
 * @return The direction of the next ray to be traced.
 */
Vec3D Renderer::computeNextRayDirection(const Ray &ray, const HitPayload &payload, uint32_t &seed,
                                         Color3D &throughput, const Color3D &surfaceColor,
                                         IMaterial::MaterialType materialType, float roughness)
{
    Vec3D rayDirection;

    // The caller resolves per-triangle material overrides. Do not look up the
    // primitive's fallback material here: mixed-material meshes must bounce
    // with the same material that was shaded.
    switch (materialType)
    {
    case IMaterial::DIFFUSE:
        // Cosine-weighted sampling is the correct distribution for a
        // Lambertian surface and reduces noise compared with uniform sampling.
        rayDirection = vec3DRandomCosineHemisphere(payload.getShadingNormal(), seed);
        break;
    case IMaterial::METALLIC:
    {
        const Vec3D viewDirection = vec3DNormalize(ray.getOrigin() - payload.getHitPosition());
        Color3D weight;

        if (!sampleMetallic(viewDirection, payload.getShadingNormal(), surfaceColor, roughness, seed, rayDirection,
                            weight))
            return Vec3D(0.0f);

        throughput *= weight;
        break;
    }
    case IMaterial::EMISSIVE:   // Return 0 light (it should have stoped befor this funcion in an if condition)
    case IMaterial::FLAT_COLOR: // (Does not bounce)
    case IMaterial::DIELECTRIC: // Handled separately in getPixelColor.
    case IMaterial::TYPE_COUNT: // None
    default:
        break;
    }
    return rayDirection;
}

/**
 * Computes the Phong contribution of one light sample.
 *
 * Ambient samples only contribute their radiance. Directional and area samples
 * combine diffuse and specular terms using the sampled direction and radiance.
 *
 * @param payload The hit payload containing information about the intersection point.
 * @param material The material of the hit primitive.
 * @param sample The sampled light connection.
 * @param ambient Whether the light is ambient.
 * @param ray The current camera or bounce ray.
 * @return The computed colour contribution.
 */
Color3D Renderer::computeLightContribution(const HitPayload &payload, const IMaterial &material,
                                           const LightSample &sample, bool ambient, const Ray &ray, float roughness)
{
    const float reflectionCoefficient = material.getAmbientReflectionCoefficient() +
                                        material.getDiffuseReflectionCoefficient() +
                                        material.getSpecularReflectionCoefficient();

    if (ambient)
        return reflectionCoefficient * sample.radiance;

    const Vec3D normal = payload.getShadingNormal();
    const float diffuseFactor = std::max(vec3DDot(normal, sample.direction), 0.0f);

    if (diffuseFactor <= 0.0f)
        return Color3D(0.0f);

    const Vec3D viewDirection = vec3DNormalize(ray.getOrigin() - payload.getHitPosition());
    const Vec3D halfDirection = vec3DNormalize(sample.direction + viewDirection);
    const float shininess = blinnShininess(roughness);
    const float specularFactor = std::pow(std::max(vec3DDot(normal, halfDirection), 0.0f), shininess);

    return reflectionCoefficient * sample.radiance * diffuseFactor +
           material.getSpecularReflectionCoefficient() * sample.radiance * specularFactor;
}

/**
 * Retrieves the color of a pixel at the specified coordinates.
 *
 * Main raytracing algorithm.
 *
 * @param x The x-coordinate of the pixel (from the image loop).
 * @param y The y-coordinate of the pixel (from the image loop).
 * @return The color of the pixel.
 */
Color4D Renderer::getPixelColor(uint32_t seed, int x, int y)
{
    Ray ray;
    int bounces = 15;
    Color3D accumulatedColor(0.0f);
    Color3D throughput(1.0f);
    uint32_t sampler = seed;
    DielectricStack dielectricStack;

    dielectricReset(dielectricStack);
    // float brightReduction = 1.5;

    const Camera &camera = *getCamera();
    Scene &scene = *getScene();
    const Primitives &primitives = scene.getPrimitives();
    const Materials &materials = scene.getMaterials();
    const Lights &lights = scene.getLights();
    const Background &background = scene.getBackground();
    const std::shared_ptr<const Texture> &environment = background.environment;
    const std::shared_ptr<const EnvironmentDistribution> &environmentDistribution = background.environmentDistribution;
    float backgroundWeight = 1.0f;
    const std::size_t pixelIndex =
        static_cast<std::size_t>(x) + static_cast<std::size_t>(y) * static_cast<std::size_t>(camera.getImageWidth());

    // Set the origin and direction of the ray. The jitter is expressed in
    // pixel units so anti-aliasing is independent of scene scale and distance.
    Vec3D direction = _rayDirections[pixelIndex];
    const float pixelRandomPosX = Utils::numericGeneratePseudoRandom<float>(sampler, -0.5f, 0.5f);
    const float pixelRandomPosY = Utils::numericGeneratePseudoRandom<float>(sampler, -0.5f, 0.5f);

    direction += pixelRandomPosX * camera.getPixelWidthDistance() +
                 pixelRandomPosY * camera.getPixelHeightDistance();

    Vec3D origin = camera.getLookFrom();
    const float lensRadius = camera.getLensRadius();

    // A zero lens radius keeps the pinhole camera and must not consume random
    // values, so existing scenes stay byte-identical.
    if (lensRadius > 0.0f && camera.getFocalDistance() > 0.0)
    {
        const float lensU = Utils::numericGeneratePseudoRandom<float>(sampler, 0.0f, 1.0f);
        const float lensV = Utils::numericGeneratePseudoRandom<float>(sampler, 0.0f, 1.0f);
        const float lensDistance = std::sqrt(lensU);
        const float lensAngle = 2.0f * PI_F * lensV;

        origin += camera.getCamViewRight() * (lensDistance * std::cos(lensAngle) * lensRadius) +
                  camera.getCamViewUp() * (lensDistance * std::sin(lensAngle) * lensRadius);

        const float focusRatio = camera.getFocusDistance() / static_cast<float>(camera.getFocalDistance());

        direction = (camera.getLookFrom() + direction * focusRatio) - origin;
    }

    ray.setOrigin(origin);
    ray.setDirection(direction);

    // Shoot the ray, bounce it around the scene, and accumulate the color of the pixel
    const Atmosphere &atmosphere = scene.getAtmosphere();

    for (int bounce = 0; bounce < bounces; ++bounce)
    {
        HitPayload payload = shotRay(ray);
        bool mediumScattered = false;

        if (atmosphereActive(atmosphere))
        {
            const Vec3D direction = ray.getDirection();
            const float directionLength = direction.length();

            if (directionLength > 0.0f)
            {
                const Vec3D unitDirection = direction / directionLength;
                float segmentDistance = atmosphere.maxDistance;

                if (payload.getHitDistance() != std::numeric_limits<float>::infinity())
                    segmentDistance = std::min(segmentDistance, payload.getHitDistance() * directionLength);

                float scatterDistance = 0.0f;
                const float distanceSample = Utils::numericGeneratePseudoRandom<float>(sampler, 0.0f, 1.0f);

                if (sampleAtmosphereDistance(atmosphere, ray.getOrigin(), unitDirection, distanceSample,
                                             scatterDistance) &&
                    scatterDistance < segmentDistance)
                {
                    const Point3D scatterPosition = ray.at(scatterDistance / directionLength);

                    for (const std::shared_ptr<ILight> &light : lights)
                    {
                        const LightSample lightSample = light->sampleMedium(scatterPosition, scene, sampler);

                        if (!lightSample.valid)
                            continue;

                        const float lightDistance = light->isAmbient()
                                                        ? atmosphere.maxDistance
                                                        : std::min(lightSample.distance, atmosphere.maxDistance);
                        const Vec3D lightDirection = light->isAmbient() ? unitDirection : lightSample.direction;
                        const float transmittance = atmosphereTransmittance(
                            atmosphere, scatterPosition, lightDirection, lightDistance);

                        if (light->isAmbient())
                        {
                            accumulatedColor +=
                                throughput * atmosphere.color * lightSample.radiance * transmittance;
                            continue;
                        }

                        const float cosine =
                            std::clamp(vec3DDot(unitDirection, lightSample.direction), -1.0f, 1.0f);
                        const float phase = henyeyGreensteinPhase(cosine, atmosphere.anisotropy);

                        accumulatedColor += throughput * atmosphere.color * lightSample.radiance *
                                            (phase * transmittance);
                    }

                    const float directionU = Utils::numericGeneratePseudoRandom<float>(sampler, 0.0f, 1.0f);
                    const float directionV = Utils::numericGeneratePseudoRandom<float>(sampler, 0.0f, 1.0f);
                    const Vec3D nextDirection =
                        sampleHenyeyGreenstein(unitDirection, atmosphere.anisotropy, directionU, directionV);

                    throughput *= atmosphere.color;
                    ray.setOrigin(scatterPosition + nextDirection * Ray::EPSILON);
                    ray.setDirection(nextDirection);
                    backgroundWeight = 1.0f;
                    mediumScattered = true;
                }
                else
                {
                    const float transmittance =
                        atmosphereTransmittance(atmosphere, ray.getOrigin(), unitDirection, segmentDistance);

                    throughput *= transmittance;
                }
            }
        }

        if (mediumScattered)
            continue;

        // If we hit an object
        if (payload.getHitDistance() != std::numeric_limits<float>::infinity())
        {
            const auto &primitive = primitives[payload.getHitIndex()];
            const int materialOverride = payload.getMaterialIndex();
            const uint materialIndex =
                materialOverride >= 0 && static_cast<std::size_t>(materialOverride) < materials.size()
                    ? static_cast<uint>(materialOverride)
                    : primitive->getMaterialIndex();
            const auto &material = materials.at(materialIndex);
            const IMaterial::MaterialType materialType = material->getType();
            Color3D surfaceColor = material->getDiffuseColor();

            if (const std::shared_ptr<const Texture> texture = material->getTexture())
                surfaceColor *= texture->sample(payload.getU(), payload.getV());

            float roughness = material->getRoughness();

            if (const std::shared_ptr<const Texture> texture = material->getRoughnessTexture())
                roughness = std::clamp(roughness * texture->sample(payload.getU(), payload.getV()).getX(), 0.0f, 1.0f);

            Color3D emissionColor = material->getEmissionColor();

            if (const std::shared_ptr<const Texture> texture = material->getEmissionTexture())
                emissionColor *= texture->sample(payload.getU(), payload.getV());

            if (materialType == IMaterial::DIELECTRIC)
            {
                const Vec3D normal = payload.getHitNormal();
                // Camera rays are not normalized. Fresnel and Snell's law
                // require a unit incident vector.
                const Vec3D incident = vec3DNormalize(ray.getDirection());

                if (vec3DIsNearZero(incident))
                    break;

                const float cosi = std::clamp(-vec3DDot(incident, normal), 0.0f, 1.0f);
                const float ior = material->getIndexOfRefraction();
                const int hitIndex = payload.getHitIndex();
                const int stackPosition = dielectricFind(dielectricStack, hitIndex);
                const bool exiting = stackPosition >= 0;
                const float etaI = exiting ? ior : dielectricSurroundingIor(dielectricStack);
                const float etaT = exiting ? dielectricExteriorIor(dielectricStack, stackPosition) : ior;
                const float eta = etaI / etaT;
                const float k = 1.0f - eta * eta * (1.0f - cosi * cosi);
                const float ratio = (etaI - etaT) / (etaI + etaT);
                const float reflectance = ratio * ratio;
                const float fresnel = reflectance + (1.0f - reflectance) * std::pow(1.0f - cosi, 5.0f);
                const bool reflectRay =
                    k < 0.0f || Utils::numericGeneratePseudoRandom<float>(sampler, 0.0f, 1.0f) < fresnel;
                Vec3D nextDirection;

                if (reflectRay)
                    nextDirection = vec3DNormalize(reflect(incident, normal));
                else
                {
                    nextDirection = vec3DNormalize(eta * incident + (eta * cosi - std::sqrt(k)) * normal);

                    if (exiting)
                        dielectricRemove(dielectricStack, hitIndex);
                    else if (!dielectricPush(dielectricStack, hitIndex, ior))
                        break;
                }

                if (vec3DIsNearZero(nextDirection))
                    break;

                throughput *= surfaceColor;

                if (throughput.lengthSquared() < 1e-6f)
                    break;

                ray.setDirection(nextDirection);
                ray.setOrigin(payload.getHitPosition() + nextDirection * Ray::EPSILON);
                backgroundWeight = 1.0f;
                continue;
            }

            if (const std::shared_ptr<const Texture> normalTexture = material->getNormalTexture())
            {
                const Color3D sample = normalTexture->sample(payload.getU(), payload.getV());
                const Vec3D normal = payload.getShadingNormal();
                const Vec3D tangent = orthonormalizeTangent(payload.getShadingTangent(), normal);

                if (tangent.lengthSquared() >= 1e-12f)
                {
                    const Vec3D bitangent = vec3DCross(normal, tangent);
                    const Vec3D mapped(2.0f * sample.getX() - 1.0f, 2.0f * sample.getY() - 1.0f,
                                       2.0f * sample.getZ() - 1.0f);
                    Vec3D perturbed = tangent * mapped.getX() + bitangent * mapped.getY() + normal * mapped.getZ();

                    if (perturbed.lengthSquared() >= 1e-12f)
                    {
                        perturbed = vec3DNormalize(perturbed);
                        if (vec3DDot(perturbed, ray.getDirection()) > 0.0f)
                            perturbed = -perturbed;
                        payload.setShadingNormal(perturbed);
                    }
                }
            }

            // Add emission for emissive materials and for materials with an
            // imported additive emission term (for example MTL Ke).
            if (emissionColor.lengthSquared() > 0.0f)
                accumulatedColor += throughput * emissionColor;

            // Compute lighting contributions from each light source
            for (size_t i = 0; i < lights.size(); ++i)
            {
                const LightSample lightSample = lights[i]->sample(payload, scene, sampler);

                if (!lightSample.valid)
                    continue;

                // Calculate Phong lighting model for the hit point and the current light
                const float lightDistance =
                    lights[i]->isAmbient() ? atmosphere.maxDistance
                                           : std::min(lightSample.distance, atmosphere.maxDistance);
                const Vec3D lightDirection =
                    lights[i]->isAmbient() ? Vec3D(0.0f, 1.0f, 0.0f) : lightSample.direction;
                const float lightTransmittance =
                    atmosphereTransmittance(atmosphere, payload.getHitPosition(), lightDirection, lightDistance);

                Color3D phongLighting =
                    computeLightContribution(payload, *material, lightSample, lights[i]->isAmbient(), ray, roughness) *
                    lightTransmittance;

                // Reduce the Phong lighting because it's too bright
                // phongLighting /= brightReduction;

                // Multiply the Phong components by the object's color (accumulate object color)
                Color3D phongColor = phongLighting * surfaceColor;

                // Accumulate the Phong color, attenuated by the path throughput.
                accumulatedColor += throughput * phongColor;
            }

            // Emissive and flat materials do not bounce. Dielectrics were
            // handled above and never reach this point.
            if (materialType == IMaterial::EMISSIVE || materialType == IMaterial::FLAT_COLOR)
                break;

            // Diffuse paths are attenuated by the surface colour. Metallic
            // paths apply their Fresnel/GGX weight inside the sampler.
            if (materialType == IMaterial::DIFFUSE)
            {
                throughput *= surfaceColor;

                if (throughput.lengthSquared() < 1e-6f)
                    break;
            }

            float nextBackgroundWeight = 1.0f;

            // Explicit environment lighting for diffuse surfaces. The
            // environment strategy is combined with cosine-weighted BSDF
            // sampling through MIS so highlights and smooth skies both stay
            // low-noise.
            if (materialType == IMaterial::DIFFUSE && background.enabled && environment && environmentDistribution &&
                environmentDistribution->total > 0.0f)
            {
                const Vec3D normal = payload.getShadingNormal();
                const EnvironmentSample environmentSample =
                    sampleEnvironment(*environment, *environmentDistribution, sampler);

                if (environmentSample.valid && environmentSample.pdf > 0.0f)
                {
                    const float environmentCosine = std::max(vec3DDot(normal, environmentSample.direction), 0.0f);

                    if (environmentCosine > 0.0f)
                    {
                        const float bsdfPdfAtEnvironment = environmentCosine / PI_F;
                        const float environmentWeight =
                            powerHeuristicPdf(environmentSample.pdf, bsdfPdfAtEnvironment);

                        const float environmentTransmittance =
                            atmosphereTransmittance(atmosphere, payload.getHitPosition(),
                                                   environmentSample.direction, atmosphere.maxDistance);

                        accumulatedColor += throughput * environmentSample.radiance *
                                            (environmentCosine / (PI_F * environmentSample.pdf)) * environmentWeight *
                                            environmentTransmittance;
                    }
                }
            }

            // Handle set direction and origin for the next ray
            const Vec3D nextDirection =
                computeNextRayDirection(ray, payload, sampler, throughput, surfaceColor, materialType, roughness);

            if (throughput.lengthSquared() < 1e-6f)
                break;

            // A zero or inward-facing direction would trace a degenerate ray
            // forever, so stop instead.
            if (vec3DIsNearZero(nextDirection) || vec3DDot(nextDirection, payload.getHitNormal()) <= 0.0f)
                break;

            if (materialType == IMaterial::DIFFUSE && background.enabled && environment && environmentDistribution &&
                environmentDistribution->total > 0.0f)
            {
                const float bsdfCosine =
                    std::max(vec3DDot(payload.getShadingNormal(), nextDirection), 0.0f);
                const float bsdfPdf = bsdfCosine / PI_F;
                const float environmentPdfValue = environmentPdf(*environment, *environmentDistribution, nextDirection);

                nextBackgroundWeight = powerHeuristicPdf(bsdfPdf, environmentPdfValue);
            }

            backgroundWeight = nextBackgroundWeight;
            ray.setDirection(nextDirection);
            ray.setOrigin(payload.getHitPosition() + payload.getHitNormal() * Ray::EPSILON);
        }
        else
        {
            if (background.enabled)
                accumulatedColor += throughput * backgroundWeight * background.sample(ray.getDirection());
            break;
        }
    }
    return Color4D(accumulatedColor, 1);
}

/**
 * Shoots a ray into the scene and returns the hit payload.
 *
 * @param ray The ray to be shot into the scene.
 * @return The hit payload containing information about the intersection.
 */
HitPayload Renderer::shotRay(const Ray &ray)
{
    HitPayload payload;
    payload.setHitDistance(std::numeric_limits<float>::infinity());

    // Handle no objects
    if (getScene()->getPrimitives().size() == 0)
        return missHit();

    //! In primitive hit, IGNORE NEGATIVE DISTANCES.
    if (getScene()->hit(ray, payload, 0))
        return payload;
    return missHit();
}

/**
 * This function is called when a ray misses all the objects in the scene.
 * It returns a HitPayload object representing the result of the miss hit.
 *
 * @return The HitPayload object representing the result of the miss hit.
 */
HitPayload Renderer::missHit(void)
{
    HitPayload payload;
    payload.setHitDistance(std::numeric_limits<float>::infinity());
    return payload;
}

} // namespace RayTracer
