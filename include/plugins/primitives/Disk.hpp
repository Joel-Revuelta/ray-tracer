/*
** EPITECH PROJECT, 2024
** raytracer
** File description:
** Disk
*/

#ifndef DISK_HPP_
#define DISK_HPP_

#include "Primitive.hpp"

namespace RayTracer
{

/** Finite disk in local space. Use InstancePrimitive to orient and place it. */
class Disk : public IPrimitive
{
  public:
    Disk(const Vec3D &center, const Vec3D &normal, float radius, float innerRadius, uint materialIndex);
    ~Disk(void);

    void doTransformation(const ITransformation &t) override;
    bool hit(const Ray &ray, HitPayload &payload, size_t i) const override;
    std::shared_ptr<IPrimitive> clone(void) const override;
    bool getData(PrimitiveData &data) const override;

    uint getMaterialIndex(void) const override;
    void setMaterialIndex(const uint &index) override;
    const Vec3D &getCenter(void) const;
    const Vec3D &getNormal(void) const;
    float getRadius(void) const;
    float getInnerRadius(void) const;

  private:
    Vec3D _center;
    Vec3D _normal;
    float _radius;
    float _innerRadius;
    uint _materialIndex;
};

} // namespace RayTracer

#endif // DISK_HPP_
