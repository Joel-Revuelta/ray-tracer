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
#include "plugins/renderers/LightData.hpp"
#include <memory>

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

    // Radiance at the hit point without the Lambert term. Used by the
    // renderers to combine diffuse and specular contributions.
    virtual Color3D getRadiance(const HitPayload &payload) const = 0;

    // Unit vector from the hit point towards the light. Ambient lights return
    // a zero vector because they have no direction.
    virtual Vec3D getLightDirection(const HitPayload &payload) const = 0;

    virtual bool isAmbient(void) const
    {
        return false;
    }

    // Returns an independent copy for scene composition. Lights that do not
    // implement cloning fall back to sharing the original instance.
    virtual std::shared_ptr<ILight> clone(void) const
    {
        return nullptr;
    }

    // Fills a renderer-neutral description of the light.
    virtual bool getData(LightData &data) const
    {
        (void)data;
        return false;
    }
};

} // namespace RayTracer

#endif // LIGHT_HPP_
