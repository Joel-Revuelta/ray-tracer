/*
** EPITECH PROJECT, 2024
** raytracer
** File description:
** InstancePrimitive
*/

#ifndef INSTANCE_PRIMITIVE_HPP_
#define INSTANCE_PRIMITIVE_HPP_

#include "Primitive.hpp"
#include "Transform.hpp"
#include <memory>

namespace RayTracer
{

/**
 * Applies an affine transform to a primitive without modifying the primitive
 * itself. Rays are transformed into the child's local space and hits are
 * converted back to world space.
 */
class InstancePrimitive : public IPrimitive
{
  public:
    InstancePrimitive(std::shared_ptr<IPrimitive> child, const TransformData &transform);

    void doTransformation(const ITransformation &t) override;
    bool hit(const Ray &ray, HitPayload &payload, size_t i) const override;
    std::shared_ptr<IPrimitive> clone(void) const override;
    bool getData(PrimitiveData &data) const override;

    uint getMaterialIndex(void) const override;
    void setMaterialIndex(const uint &index) override;

    const TransformData &getTransform(void) const;

  private:
    std::shared_ptr<IPrimitive> _child;
    TransformData _transform;
};

} // namespace RayTracer

#endif // INSTANCE_PRIMITIVE_HPP_
