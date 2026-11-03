/*
** EPITECH PROJECT, 2023
** raytracer
** File description:
** DirectionalLight.cpp
*/

#include "DirectionalLight.hpp"
#include "Color.hpp"
#include "Vec3D.hpp"
#include <algorithm>

using namespace RayTracer;

DirectionalLight::DirectionalLight(const Vec3D &direction, float intensity)
    : _color(1.0, 1.0, 1.0), _direction(direction), _intensity(intensity)
{
}

DirectionalLight::DirectionalLight(const Vec3D &direction, const Color3D &color, float intensity)
    : _color(color), _direction(direction), _intensity(intensity)
{
}

DirectionalLight::~DirectionalLight(void)
{
}

void DirectionalLight::doTransformation(const ITransformation &t)
{
    t.doTransformation(*this);
}

std::shared_ptr<ILight> DirectionalLight::clone(void) const
{
    return std::make_shared<DirectionalLight>(*this);
}

bool DirectionalLight::getData(LightData &data) const
{
    data.type = LightType::DIRECTIONAL;
    data.direction = _direction;
    data.color = _color;
    data.intensity = _intensity;
    return true;
}

/**
 * Calculates the lighting contribution from the directional light source.
 *
 * @param payload The hit payload containing information about the intersection point.
 * @return The color of the lighting contribution.
 */
Color3D DirectionalLight::calculateLighting(const HitPayload &payload) const
{
    const float diffuseFactor =
        std::max<float>(vec3DDot(getLightDirection(payload), payload.getShadingNormal()), 0.0f);

    return diffuseFactor * getRadiance(payload);
}

Color3D DirectionalLight::getRadiance(const HitPayload &payload) const
{
    (void)payload;
    return getColor() * getIntensity();
}

Vec3D DirectionalLight::getLightDirection(const HitPayload &payload) const
{
    (void)payload;
    return vec3DNormalize(getDirection());
}

bool DirectionalLight::isPointInShadow(const HitPayload &payload, const Scene &scene) const
{
    HitPayload possibleHitInfo;
    Vec3D offset = payload.getHitNormal() * Ray::EPSILON;

    // Create a shadow ray from the hitpoint in the direction of the light
    Ray shadowRay(payload.getHitPosition() + offset, vec3DNormalize(getDirection()));

    // Send the shadow ray to the objects in the scene
    scene.hit(shadowRay, possibleHitInfo, 0);

    // Check if the shadow has hit anything
    if (possibleHitInfo.getHitDistance() == std::numeric_limits<float>::infinity())
        return false;
    return true;
}

Color3D DirectionalLight::getColor(void) const
{
    return _color;
}

Vec3D DirectionalLight::getDirection(void) const
{
    return _direction;
}

float DirectionalLight::getIntensity(void) const
{
    return _intensity;
}

void DirectionalLight::setIntensity(float intensity)
{
    _intensity = intensity;
}

void DirectionalLight::setColor(const Color3D &color)
{
    _color = color;
}

void DirectionalLight::setDirection(const Vec3D &direction)
{
    _direction = direction;
}

extern "C" std::shared_ptr<ILight> create(Vec3D direction, Color3D color, double intensity)
{
    return std::make_shared<DirectionalLight>(direction, color, intensity);
}
