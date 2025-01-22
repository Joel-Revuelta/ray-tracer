/*
** EPITECH PROJECT, 2024
** raytracer
** File description:
** Cylinder.hpp
*/

#ifndef CYLINDER_HPP_
#define CYLINDER_HPP_

#include "Primitive.hpp"

namespace RayTracer
{
class Cylinder : public IPrimitive
{
  public:
    Cylinder(const float radius, const Vec3D &center, const float height, uint materialIndex);
    ~Cylinder();

    void doTransformation(const ITransformation &t) override;
    bool hit(const Ray &ray, HitPayload &payload, size_t i) const override;
    uint getMaterialIndex(void) const override;
    void setMaterialIndex(const uint &index) override;

    void setCenter(const Vec3D &center);
    void setRadius(const double radius);
    void setHeight(const double height);
    void setAxis(const Vec3D &axis);

    Vec3D getCenter(void) const;
    float getRadius(void) const;
    float getHeight(void) const;
    Vec3D getAxis(void) const;

  private:
    double _radius;
    Point3D _center;
    double _height;
    uint _materialIndex;
    Vec3D _axis;
};
} // namespace RayTracer

#endif /* !CYLINDER_HPP_ */
