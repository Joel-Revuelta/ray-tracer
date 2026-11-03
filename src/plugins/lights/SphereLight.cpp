/*
** EPITECH PROJECT, 2024
** raytracer
** File description:
** SphereLight
*/

#include "SphereLight.hpp"
#include "Ray.hpp"
#include "Scene.hpp"
#include "Transform.hpp"
#include "Utils.hpp"
#include <algorithm>
#include <cmath>
#include <stdexcept>

using namespace RayTracer;

SphereLight::SphereLight(const Vec3D &position, float radius, const Color3D &color, float intensity,
                         float attenuationConstant, float attenuationLinear, float attenuationQuadratic)
    : _position(position), _radius(radius), _color(color), _intensity(intensity),
      _attenuationConstant(attenuationConstant), _attenuationLinear(attenuationLinear),
      _attenuationQuadratic(attenuationQuadratic)
{
    if (!std::isfinite(_radius) || _radius <= 0.0f)
        throw std::invalid_argument("Sphere light radius must be positive");
    if (!std::isfinite(_intensity) || _intensity < 0.0f)
        throw std::invalid_argument("Sphere light intensity must not be negative");
    if (!std::isfinite(_attenuationConstant) || _attenuationConstant <= 0.0f)
        throw std::invalid_argument("Sphere light attenuation constant must be positive");
    if (!std::isfinite(_attenuationLinear) || _attenuationLinear < 0.0f ||
        !std::isfinite(_attenuationQuadratic) || _attenuationQuadratic < 0.0f)
        throw std::invalid_argument("Sphere light attenuation must not be negative");
}

SphereLight::~SphereLight(void)
{
}

void SphereLight::doTransformation(const ITransformation &t)
{
    t.doTransformation(*this);
}

void SphereLight::applyTransform(const TransformData &transform)
{
    _position = transformPoint(transform, _position);
    _radius *= transformScaleFactor(transform);
    _radius = std::max(_radius, 1e-6f);
}

float SphereLight::attenuationAt(float distance) const
{
    const float denominator = _attenuationConstant + _attenuationLinear * distance +
                              _attenuationQuadratic * distance * distance;

    return denominator > 0.0f ? 1.0f / denominator : 0.0f;
}

Color3D SphereLight::calculateLighting(const HitPayload &payload) const
{
    const float diffuseFactor =
        std::max<float>(vec3DDot(getLightDirection(payload), payload.getShadingNormal()), 0.0f);

    return diffuseFactor * getRadiance(payload);
}

Color3D SphereLight::getRadiance(const HitPayload &payload) const
{
    const float distance = (_position - payload.getHitPosition()).length();

    return _color * _intensity * attenuationAt(distance);
}

Vec3D SphereLight::getLightDirection(const HitPayload &payload) const
{
    return vec3DNormalize(_position - payload.getHitPosition());
}

bool SphereLight::isPointInShadow(const HitPayload &payload, const Scene &scene) const
{
    const Vec3D toLight = _position - payload.getHitPosition();
    const float distance = toLight.length();

    if (distance <= 1e-6f)
        return true;

    const Vec3D direction = toLight / distance;
    const Ray shadowRay(payload.getHitPosition() + payload.getHitNormal() * Ray::EPSILON, direction);
    HitPayload occluder;

    scene.hit(shadowRay, occluder, 0);
    return occluder.getHitDistance() < distance - Ray::EPSILON;
}

LightSample SphereLight::sample(const HitPayload &payload, const Scene &scene, uint32_t &seed) const
{
    LightSample result;

    // Uniform point on the sphere surface. Samples below the surface are
    // rejected by the shading-normal test, which keeps the estimator unbiased.
    const float u = Utils::numericGeneratePseudoRandom<float>(seed, 0.0f, 1.0f);
    const float v = Utils::numericGeneratePseudoRandom<float>(seed, 0.0f, 1.0f);
    const float z = 1.0f - 2.0f * u;
    const float radial = std::sqrt(std::max(0.0f, 1.0f - z * z));
    const float angle = 2.0f * static_cast<float>(Utils::PI) * v;
    const Vec3D sphereNormal(radial * std::cos(angle), radial * std::sin(angle), z);
    const Vec3D lightPoint = _position + sphereNormal * _radius;
    const Vec3D toLight = lightPoint - payload.getHitPosition();
    const float distance = toLight.length();

    if (!std::isfinite(distance) || distance <= 1e-6f)
        return result;

    const Vec3D direction = toLight / distance;

    if (vec3DDot(direction, payload.getShadingNormal()) <= 0.0f)
        return result;

    const Ray shadowRay(payload.getHitPosition() + payload.getHitNormal() * Ray::EPSILON, direction);
    HitPayload occluder;

    scene.hit(shadowRay, occluder, 0);

    if (occluder.getHitDistance() < distance - Ray::EPSILON)
        return result;

    result.direction = direction;
    result.radiance = _color * _intensity * attenuationAt(distance);
    result.valid = true;
    return result;
}

bool SphereLight::getData(LightData &data) const
{
    data.type = LightType::AREA_SPHERE;
    data.position = _position;
    data.radius = _radius;
    data.color = _color;
    data.intensity = _intensity;
    data.attenuationConstant = _attenuationConstant;
    data.attenuationLinear = _attenuationLinear;
    data.attenuationQuadratic = _attenuationQuadratic;
    return true;
}

std::shared_ptr<ILight> SphereLight::clone(void) const
{
    return std::make_shared<SphereLight>(*this);
}

const Vec3D &SphereLight::getPosition(void) const
{
    return _position;
}

float SphereLight::getRadius(void) const
{
    return _radius;
}

const Color3D &SphereLight::getColor(void) const
{
    return _color;
}

float SphereLight::getIntensity(void) const
{
    return _intensity;
}

extern "C" std::shared_ptr<ILight> create(Vec3D position, double radius, Color3D color, double intensity,
                                          double attenuationConstant, double attenuationLinear,
                                          double attenuationQuadratic)
{
    return std::make_shared<SphereLight>(position, static_cast<float>(radius), color, static_cast<float>(intensity),
                                         static_cast<float>(attenuationConstant),
                                         static_cast<float>(attenuationLinear),
                                         static_cast<float>(attenuationQuadratic));
}
