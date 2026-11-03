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
#include <vector>

namespace RayTracer
{

class MeshAsset;

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

    // Returns the shared geometry for mesh primitives. Instance wrappers
    // delegate so the scene snapshot can deduplicate assets.
    virtual std::shared_ptr<const MeshAsset> getMeshAsset(void) const
    {
        return nullptr;
    }

    // Resolved OBJ material groups for mesh primitives, aligned with
    // MeshAsset::materialNames. -1 entries fall back to the primitive
    // material. Non-mesh primitives return nullptr.
    virtual std::shared_ptr<const std::vector<int>> getMeshMaterialMap(void) const
    {
        return nullptr;
    }

    // Shifts material indices when a nested scene is merged into a parent.
    virtual void shiftMaterialIndices(uint offset)
    {
        (void)offset;
    }

    virtual uint getMaterialIndex(void) const = 0;

    virtual void setMaterialIndex(const uint &index) = 0;
};

} // namespace RayTracer

#endif // PRIMITIVE_HPP_
