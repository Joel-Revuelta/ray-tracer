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
#include "TransformData.hpp"
#include "Transformable.hpp"
#include "plugins/renderers/LightData.hpp"
#include <cstdint>
#include <memory>

namespace RayTracer
{

class Scene;

/**
 * One sampled connection from a surface point to a light.
 *
 * `direction` points from the surface towards the light, `radiance` already
 * includes the light's attenuation, and `valid` is false when the sample is
 * below the surface or occluded.
 */
struct LightSample
{
    Vec3D direction;
    Color3D radiance;
    bool valid = false;
};

class ILight : public ITransformable
{
  public:
    virtual ~ILight(void) = default;

    virtual void doTransformation(const ITransformation &t) = 0;

    // Applies a data-driven transform when a scene is composed as a group or
    // prefab instance. Lights that have no position or direction ignore it.
    virtual void applyTransform(const TransformData &transform)
    {
        (void)transform;
    }

    virtual Color3D calculateLighting(const HitPayload &payload) const = 0;
    virtual bool isPointInShadow(const HitPayload &payload, const Scene &scene) const = 0;

    // Radiance at the hit point without the Lambert term. Used by the
    // renderers to combine diffuse and specular contributions.
    virtual Color3D getRadiance(const HitPayload &payload) const = 0;

    // Unit vector from the hit point towards the light. Ambient lights return
    // a zero vector because they have no direction.
    virtual Vec3D getLightDirection(const HitPayload &payload) const = 0;

    // Samples one connection to the light, including visibility. The default
    // implementation keeps point, directional and ambient lights unchanged and
    // consumes no random values. Area lights override it.
    virtual LightSample sample(const HitPayload &payload, const Scene &scene, uint32_t &seed) const
    {
        (void)seed;

        LightSample result;

        if (isPointInShadow(payload, scene))
            return result;

        result.direction = getLightDirection(payload);
        result.radiance = getRadiance(payload);
        result.valid = true;
        return result;
    }

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
