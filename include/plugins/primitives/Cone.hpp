/*
** EPITECH PROJECT, 2024
** raytracer
** File description:
** Cone
*/

#ifndef CONE_HPP_
#define CONE_HPP_

#include "Primitive.hpp"

namespace RayTracer
{
class Cone : public IPrimitive
{
  public:
    Cone(const float radius, const Vec3D &center, const float height, uint materialIndex);
    ~Cone();

    void doTransformation(const ITransformation &t) override;
    bool hit(const Ray &ray, HitPayload &payload, size_t i) const override;
    uint getMaterialIndex(void) const override;
    void setMaterialIndex(const uint &index) override;

    void setCenter(const Vec3D &center);
    void setRadius(const double radius);
    void setHeight(const double height);

    Vec3D getCenter(void) const;
    float getRadius(void) const;
    float getHeight(void) const;
    bool rotation(const Ray &ray, HitPayload &payload, size_t i) const;
    void setRotation(bool r);

  private:
    double _radius;
    Point3D _center;
    double _height;
    uint _materialIndex;
    bool _rotation;
};
} // namespace RayTracer

#endif /* !CONE_HPP_ */
