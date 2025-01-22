/*
** EPITECH PROJECT, 2023
** raytracer
** File description:
** DirectionalLight.hpp
*/

#ifndef DIRECTIONALLIGHT_HPP_
#define DIRECTIONALLIGHT_HPP_

#include "Color.hpp"
#include "Light.hpp"
#include "Scene.hpp"

namespace RayTracer
{
class DirectionalLight : public ILight
{
  public:
    DirectionalLight(const Vec3D &direction, float intensity);
    DirectionalLight(const Vec3D &direction, const Color3D &color, float intensity);
    ~DirectionalLight(void);

    void doTransformation(const ITransformation &t) override;
    Color3D calculateLighting(const HitPayload &payload) const override;
    bool isPointInShadow(const HitPayload &payload, const Scene &scene) const override;

    Color3D getColor(void) const;
    Vec3D getDirection(void) const;
    float getIntensity(void) const;

    void setIntensity(float intensity);
    void setColor(const Color3D &color);
    void setDirection(const Vec3D &direction);

  private:
    // Color of the light
    Color3D _color;

    // Direction of the light
    Vec3D _direction;

    // Controls the brightness of the light
    float _intensity;
};
} // namespace RayTracer

#endif // DIRECTIONALLIGHT_HPP_
