/*
** EPITECH PROJECT, 2023
** raytracer
** File description:
** Sphere.hpp
*/

#ifndef SPHERE_HPP_
#define SPHERE_HPP_

#include "Primitive.hpp"

namespace RayTracer
{

// Sphere primitive
class Sphere : public IPrimitive
{
  public:
    Sphere(const float radius, const Vec3D &center, uint _materialIndex);
    ~Sphere(void);

    void doTransformation(const ITransformation &t) override;
    bool hit(const Ray &ray, HitPayload &payload, size_t i) const override;

    void setRadius(const float radius);
    void setCenter(const Point3D center);
    void setMaterialIndex(const uint &index) override;

    float getRadius(void) const;
    Point3D getCenter(void) const;
    uint getMaterialIndex(void) const override;

  private:
    float _radius;
    Point3D _center;
    uint _materialIndex;
};

} // namespace RayTracer

#endif // SPHERE_HPP_
