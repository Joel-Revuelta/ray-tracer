/*
** EPITECH PROJECT, 2024
** raytracer
** File description:
** SphereLight
*/

#ifndef SPHERE_LIGHT_HPP_
#define SPHERE_LIGHT_HPP_

#include "Light.hpp"
#include "Vec3D.hpp"

namespace RayTracer
{

/**
 * Spherical area light. Each shading query samples a point on the sphere,
 * which produces soft shadows whose softness scales with the radius.
 */
class SphereLight : public ILight
{
  public:
    SphereLight(const Vec3D &position, float radius, const Color3D &color, float intensity,
                float attenuationConstant, float attenuationLinear, float attenuationQuadratic);
    ~SphereLight(void) override;

    void doTransformation(const ITransformation &t) override;
    void applyTransform(const TransformData &transform) override;
    Color3D calculateLighting(const HitPayload &payload) const override;
    bool isPointInShadow(const HitPayload &payload, const Scene &scene) const override;
    Color3D getRadiance(const HitPayload &payload) const override;
    Vec3D getLightDirection(const HitPayload &payload) const override;
    LightSample sample(const HitPayload &payload, const Scene &scene, uint32_t &seed) const override;
    bool getData(LightData &data) const override;
    std::shared_ptr<ILight> clone(void) const override;

    const Vec3D &getPosition(void) const;
    float getRadius(void) const;
    const Color3D &getColor(void) const;
    float getIntensity(void) const;

  private:
    float attenuationAt(float distance) const;

    Vec3D _position;
    float _radius;
    Color3D _color;
    float _intensity;
    float _attenuationConstant;
    float _attenuationLinear;
    float _attenuationQuadratic;
};

} // namespace RayTracer

#endif // SPHERE_LIGHT_HPP_
