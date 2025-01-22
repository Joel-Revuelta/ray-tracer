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

    void doTransformation(ITransformable &obj) override;
    void doTransformationPrimitive(ITransformable &obj) override;
    void doTransformationScene(ITransformable &obj) override;
    void doTransformationLight(ITransformable &obj) override;

    void doRotationSphere(Sphere &sphere);
    void doRotationCylinder(Cylinder &cylinder);
    void doRotationCone(Cone &cone);
    void doRotationPlane(Plane &plane);

  protected:
  private:
    Vec3D _rotation;
};

} // namespace RayTracer

#endif /* !ROTATION_HPP_ */
