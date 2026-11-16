/*
** EPITECH PROJECT, 2024
** raytracer
** File description:
** Box
*/

#ifndef BOX_HPP_
#define BOX_HPP_

#include "Primitive.hpp"

namespace RayTracer
{

/** Axis-aligned box in local space. Use InstancePrimitive to place it. */
class Box : public IPrimitive
{
  public:
    Box(const Vec3D &minimum, const Vec3D &maximum, uint materialIndex);
    ~Box(void);

    void doTransformation(const ITransformation &t) override;
    bool hit(const Ray &ray, HitPayload &payload, size_t i) const override;
    std::shared_ptr<IPrimitive> clone(void) const override;
    bool getData(PrimitiveData &data) const override;

    uint getMaterialIndex(void) const override;
    void setMaterialIndex(const uint &index) override;
    const Vec3D &getMinimum(void) const;
    const Vec3D &getMaximum(void) const;

  private:
    Vec3D _minimum;
    Vec3D _maximum;
    uint _materialIndex;
};

} // namespace RayTracer

#endif // BOX_HPP_
