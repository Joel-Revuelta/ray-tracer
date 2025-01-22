/*
** EPITECH PROJECT, 2023
** raytracer
** File description:
** Point.cpp
*/

#include "PointLight.hpp"
#include "HitPayload.hpp"
#include "Ray.hpp"
#include "Vec3D.hpp"
#include <algorithm>
#include <iostream>

using namespace RayTracer;

PointLight::PointLight(const Vec3D &position, const float intensity, const float attenuationConstant,
                       const float attenuationLinear, const float attenuationQuadratic)
    : _color(1.0, 1.0, 1.0), _position(position), _intensity(intensity), _attenuationConstant(attenuationConstant),
      _attenuationLinear(attenuationLinear), _attenuationQuadratic(attenuationQuadratic)
{
}

PointLight::PointLight(const Vec3D &position, const Color3D &color, const float intensity,
                       const float attenuationConstant, const float attenuationLinear, const float attenuationQuadratic)
    : _color(color), _position(position), _intensity(intensity), _attenuationConstant(attenuationConstant),
      _attenuationLinear(attenuationLinear), _attenuationQuadratic(attenuationQuadratic)
{
}

PointLight::~PointLight(void)
{
}

void PointLight::doTransformation(const ITransformation &t)
{
    (void)t;
    return;
}

Color3D PointLight::calculateLighting(const HitPayload &payload) const
{
    // Compute light direction and distance
    Vec3D lightDirection = vec3DNormalize(getPosition() - payload.getHitPosition());
    float distance = (getPosition() - payload.getHitPosition()).length();

    // Compute attenuation
    float attenuation = 0.1 * (getAttenuationConstant() + getAttenuationLinear() * distance +
                               getAttenuationQuadratic() * distance * distance);

    // Compute diffuse
    float diffuseFactor = vec3DDot(lightDirection, payload.getHitNormal());
    Color3D diffuseContrib = std::max<double>(diffuseFactor, 0.0) * getColor() * getIntensity() * attenuation;

    // Return the combined lighting contributions
    return diffuseContrib;
}

bool PointLight::isPointInShadow(const HitPayload &payload, const Scene &scene) const
{
    HitPayload possibleHitInfo;
    Vec3D offset = payload.getHitNormal() * Ray::EPSILON;

    // Create a shadow ray from the hitpoint in the direction of the light
    Ray shadowRay(payload.getHitPosition() + offset, vec3DNormalize(getPosition() - payload.getHitPosition()));

    // Send the shadow ray to the objects in the scene
    scene.hit(shadowRay, possibleHitInfo, 0);

    // Check if the shadow has hit anything
    if (possibleHitInfo.getHitDistance() == std::numeric_limits<float>::infinity())
        return false;
    return true;
}

void PointLight::setIntensity(float intensity)
{
    _intensity = intensity;
}

void PointLight::setColor(const Color3D &color)
{
    _color = color;
}

void PointLight::setPosition(const Vec3D &position)
{
    _position = position;
}

void PointLight::setAttenuationConstant(float attenuationConstant)
{
    _attenuationConstant = attenuationConstant;
}

void PointLight::setAttenuationLinear(float attenuationLinear)
{
    _attenuationLinear = attenuationLinear;
}

void PointLight::setAttenuationQuadratic(float attenuationQuadratic)
{
    _attenuationQuadratic = attenuationQuadratic;
}

Color3D PointLight::getColor(void) const
{
    return _color;
}

float PointLight::getIntensity(void) const
{
    return _intensity;
}

Vec3D PointLight::getPosition(void) const
{
    return _position;
}

float PointLight::getAttenuationConstant(void) const
{
    return _attenuationConstant;
}

float PointLight::getAttenuationLinear(void) const
{
    return _attenuationLinear;
}

float PointLight::getAttenuationQuadratic(void) const
{
    return _attenuationQuadratic;
}

extern "C" std::shared_ptr<ILight> create(const Vec3D &position, const Color3D &color, const double intensity,
                                          const double attenuationConstant, const double attenuationLinear,
                                          const double attenuationQuadratic)
{
    return std::make_shared<PointLight>(position, color, intensity, attenuationConstant, attenuationLinear,
                                        attenuationQuadratic);
}
