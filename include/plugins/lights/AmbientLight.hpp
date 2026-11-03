/*
** EPITECH PROJECT, 2023
** raytracer
** File description:
** AmbientLight.hpp
*/

#ifndef AMBIENTLIGHT_HPP_
#define AMBIENTLIGHT_HPP_

#include "Color.hpp"
#include "Light.hpp"

namespace RayTracer
{

class AmbientLight : public ILight
{
  public:
    AmbientLight(float intensity);
    AmbientLight(const Color3D &color, float intensity);
    ~AmbientLight(void);

    void doTransformation(const ITransformation &t) override;
    void applyTransform(const TransformData &transform) override;
    std::shared_ptr<ILight> clone(void) const override;
    Color3D calculateLighting(const HitPayload &payload) const override;
    bool isPointInShadow(const HitPayload &payload, const Scene &scene) const override;
    Color3D getRadiance(const HitPayload &payload) const override;
    Vec3D getLightDirection(const HitPayload &payload) const override;
    bool isAmbient(void) const override;
    bool getData(LightData &data) const override;

    Color3D getColor(void) const;
    float getIntensity(void) const;

    void setIntensity(float intensity);
    void setColor(const Color3D &color);

  private:
    // Color of the light
    Color3D _color;

    // Controls the brightness of the light
    float _intensity;
};

} // namespace RayTracer

#endif // AMBIENTLIGHT_HPP_
