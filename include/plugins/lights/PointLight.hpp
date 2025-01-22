/*
** EPITECH PROJECT, 2023
** raytracer
** File description:
** PointLight.hpp
*/

#ifndef POINTLIGHT_HPP_
#define POINTLIGHT_HPP_

#include "Color.hpp"
#include "Light.hpp"
#include "Scene.hpp"

namespace RayTracer
{

class PointLight : public ILight
{
  public:
    PointLight(const Vec3D &position, const float intensity, const float attenuationConstant,
               const float attenuationLinear, const float attenuationQuadratic);
    PointLight(const Vec3D &position, const Color3D &color, const float intensity, const float attenuationConstant,
               const float attenuationLinear, const float attenuationQuadratic);
    ~PointLight(void);

    void doTransformation(const ITransformation &t) override;
    Color3D calculateLighting(const HitPayload &payload) const override;
    bool isPointInShadow(const HitPayload &payload, const Scene &scene) const override;

    Color3D getColor(void) const;
    Vec3D getPosition(void) const;
    float getIntensity(void) const;
    float getAttenuationConstant(void) const;
    float getAttenuationLinear(void) const;
    float getAttenuationQuadratic(void) const;

    void setIntensity(float intensity);
    void setColor(const Color3D &color);
    void setPosition(const Vec3D &position);
    void setAttenuationConstant(float attenuationConstant);
    void setAttenuationLinear(float attenuationLinear);
    void setAttenuationQuadratic(float attenuationQuadratic);

  private:
    // Color of the light
    Color3D _color;

    // Position of the light
    Vec3D _position;

    // Controls the brightness of the light
    float _intensity;

    // Controls overall strength of attenuation regardless of distance
    float _attenuationConstant;

    // Determines how intensity decreases linearly with distance from the light source
    float _attenuationLinear;

    // Models additional decrease in intensity with distance squared
    float _attenuationQuadratic;
};

} // namespace RayTracer

#endif // POINTLIGHT_HPP_
