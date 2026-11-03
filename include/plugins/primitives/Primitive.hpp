/*
** EPITECH PROJECT, 2023
** raytracer
** File description:
** IPrimitive.hpp
*/

#ifndef PRIMITIVE_HPP_
#define PRIMITIVE_HPP_

#include "HitPayload.hpp"
#include "Ray.hpp"
#include "Transformable.hpp"
#include "plugins/renderers/PrimitiveData.hpp"
#include <memory>

namespace RayTracer
{

class IPrimitive : public ITransformable
{
  public:
    virtual ~IPrimitive(void) = default;

    virtual bool hit(const Ray &ray, HitPayload &payload, size_t i) const = 0;

    // Returns an independent copy for scene composition. Primitive plugins that
    // do not implement cloning fall back to sharing the original instance.
    virtual std::shared_ptr<IPrimitive> clone(void) const
    {
        return nullptr;
    }

    // Fills a renderer-neutral description of the primitive. Renderers that
    // work on a flat scene representation use this instead of dynamic_cast.
    virtual bool getData(PrimitiveData &data) const
    {
        (void)data;
        return false;
    }

    virtual uint getMaterialIndex(void) const = 0;

    virtual void setMaterialIndex(const uint &index) = 0;
};

} // namespace RayTracer

#endif // PRIMITIVE_HPP_
