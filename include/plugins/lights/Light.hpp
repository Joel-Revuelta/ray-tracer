/*
** EPITECH PROJECT, 2023
** raytracer
** File description:
** ILight.hpp
*/

#ifndef LIGHT_HPP_
#define LIGHT_HPP_

#include "Color.hpp"
#include "HitPayload.hpp"
#include "Point3D.hpp"
#include "Transformable.hpp"

namespace RayTracer
{

class Scene;

class ILight : public ITransformable
{
  public:
    virtual ~ILight(void) = default;

    virtual void doTransformation(const ITransformation &t) = 0;
    virtual Color3D calculateLighting(const HitPayload &payload) const = 0;
    virtual bool isPointInShadow(const HitPayload &payload, const Scene &scene) const = 0;
};

} // namespace RayTracer

#endif // LIGHT_HPP_
