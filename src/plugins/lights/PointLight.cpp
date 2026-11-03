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
    t.doTransformation(*this);
}

std::shared_ptr<ILight> PointLight::clone(void) const
{
    return std::make_shared<PointLight>(*this);
}

bool PointLight::getData(LightData &data) const
{
    data.type = LightType::POINT;
    data.position = _position;
    data.color = _color;
    data.intensity = _intensity;
    data.attenuationConstant = _attenuationConstant;
    data.attenuationLinear = _attenuationLinear;
    data.attenuationQuadratic = _attenuationQuadratic;
    return true;
}

Color3D PointLight::calculateLighting(const HitPayload &payload) const
{
    const float diffuseFactor =
        std::max<float>(vec3DDot(getLightDirection(payload), payload.getShadingNormal()), 0.0f);

    return diffuseFactor * getRadiance(payload);
}

Color3D PointLight::getRadiance(const HitPayload &payload) const
{
    const float distance = (getPosition() - payload.getHitPosition()).length();
    const float denominator = getAttenuationConstant() + getAttenuationLinear() * distance +
                              getAttenuationQuadratic() * distance * distance;
    const float attenuation = denominator > 0.0f ? 1.0f / denominator : 0.0f;

    return getColor() * getIntensity() * attenuation;
}

Vec3D PointLight::getLightDirection(const HitPayload &payload) const
{
    return vec3DNormalize(getPosition() - payload.getHitPosition());
}

bool PointLight::isPointInShadow(const HitPayload &payload, const Scene &scene) const
{
    HitPayload possibleHitInfo;
    Vec3D offset = payload.getHitNormal() * Ray::EPSILON;

    // Create a shadow ray from the hitpoint in the direction of the light
    Ray shadowRay(payload.getHitPosition() + offset, vec3DNormalize(getPosition() - payload.getHitPosition()));
    const float lightDistance = (getPosition() - payload.getHitPosition()).length();

    // Send the shadow ray to the objects in the scene
    scene.hit(shadowRay, possibleHitInfo, 0);

    // Only an occluder closer than the light can cast a shadow
    if (possibleHitInfo.getHitDistance() >= lightDistance)
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

extern "C" std::shared_ptr<ILight> create(Vec3D position, Color3D color, double intensity,
                                          double attenuationConstant, double attenuationLinear,
                                          double attenuationQuadratic)
{
    return std::make_shared<PointLight>(position, color, intensity, attenuationConstant, attenuationLinear,
                                        attenuationQuadratic);
}
