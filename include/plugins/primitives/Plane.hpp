/*
** EPITECH PROJECT, 2024
** raytracer
** File description:
** Plane.hpp
*/

#ifndef PLANE_HPP_
#define PLANE_HPP_

#include "Primitive.hpp"

namespace RayTracer
{
class Plane : public IPrimitive
{
  public:
    Plane(const Vec3D &normal, const Point3D &point, uint _materialIndex);
    ~Plane(void);

    void doTransformation(const ITransformation &t) override;
    bool hit(const Ray &ray, HitPayload &payload, size_t i) const override;

    void setNormal(const Vec3D &normal);
    void setPoint(const Point3D &point);
    void setMaterialIndex(const uint &index) override;

    Vec3D getNormal(void) const;
    Point3D getPoint(void) const;
    uint getMaterialIndex(void) const override;

  private:
    Vec3D _normal;
    Point3D _point;
    uint _materialIndex;
};

} // namespace RayTracer

#endif // PLANE_HPP_
