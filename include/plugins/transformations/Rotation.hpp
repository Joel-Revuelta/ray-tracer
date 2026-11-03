/*
** EPITECH PROJECT, 2024
** raytracer
** File description:
** Rotation
*/

#ifndef ROTATION_HPP_
#define ROTATION_HPP_

#include "Cone.hpp"
#include "Cylinder.hpp"
#include "Plane.hpp"
#include "Primitive.hpp"
#include "Scene.hpp"
#include "Sphere.hpp"
#include "Transformation.hpp"
#include "Vec3D.hpp"

namespace RayTracer
{
class Rotation : public ITransformation
{
  public:
    Rotation();
    Rotation(Vec3D rotation);
    ~Rotation();

    void doTransformation(ITransformable &obj) const override;
    void doTransformationPrimitive(ITransformable &obj) const override;
    void doTransformationScene(ITransformable &obj) const override;
    void doTransformationLight(ITransformable &obj) const override;

    void doRotationSphere(Sphere &sphere) const;
    void doRotationCylinder(Cylinder &cylinder) const;
    void doRotationCone(Cone &cone) const;
    void doRotationPlane(Plane &plane) const;

  protected:
  private:
    Vec3D _rotation;
};

} // namespace RayTracer

#endif /* !ROTATION_HPP_ */
