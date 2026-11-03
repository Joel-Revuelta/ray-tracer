/*
** EPITECH PROJECT, 2024
** raytracer
** File description:
** AmbientLight
*/

#include "AmbientLight.hpp"
#include "HitPayload.hpp"

using namespace RayTracer;

AmbientLight::AmbientLight(float intensity) : _color(1.0, 1.0, 1.0), _intensity(intensity)
{
}

AmbientLight::AmbientLight(const Color3D &color, float intensity) : _color(color), _intensity(intensity)
{
}

AmbientLight::~AmbientLight(void)
{
}

void AmbientLight::doTransformation(const ITransformation &t)
{
    t.doTransformation(*this);
}

std::shared_ptr<ILight> AmbientLight::clone(void) const
{
    return std::make_shared<AmbientLight>(*this);
}

bool AmbientLight::getData(LightData &data) const
{
    data.type = LightType::AMBIENT;
    data.color = _color;
    data.intensity = _intensity;
    return true;
}

/**
 * Calculates the lighting contribution of the ambient light.
 *
 * @param payload The hit payload containing information about the intersection point and surface properties.
 *
 * Note: Even though the payload is passed, the ambient light does not use any of the information in the payload.
 *
 * @return The color representing the ambient lighting contribution.
 */
Color3D AmbientLight::calculateLighting(const HitPayload &payload) const
{
    (void)payload;
    return getColor() * getIntensity();
}

Color3D AmbientLight::getRadiance(const HitPayload &payload) const
{
    (void)payload;
    return getColor() * getIntensity();
}

Vec3D AmbientLight::getLightDirection(const HitPayload &payload) const
{
    (void)payload;
    return Vec3D(0.0f);
}

bool AmbientLight::isAmbient(void) const
{
    return true;
}

bool AmbientLight::isPointInShadow(const HitPayload &payload, const Scene &scene) const
{
    (void)scene;
    (void)payload;
    return false;
}

Color3D AmbientLight::getColor(void) const
{
    return _color;
}

float AmbientLight::getIntensity(void) const
{
    return _intensity;
}

void AmbientLight::setIntensity(float intensity)
{
    _intensity = intensity;
}

void AmbientLight::setColor(const Color3D &color)
{
    _color = color;
}

extern "C" std::shared_ptr<ILight> create(Color3D color, double intensity)
{
    return std::make_shared<AmbientLight>(color, intensity);
}
