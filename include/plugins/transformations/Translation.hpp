/*
** EPITECH PROJECT, 2024
** raytracer
** File description:
** Translation.hpp
*/

#ifndef TRANSLATION_HPP_
#define TRANSLATION_HPP_

#include "Cylinder.hpp"
#include "Light.hpp"
#include "Plane.hpp"
#include "Primitive.hpp"
#include "Scene.hpp"
#include "Sphere.hpp"
#include "Transformation.hpp"
#include "Vec3D.hpp"

namespace RayTracer
{
class Translation : public ITransformation
{
  public:
    Translation();
    Translation(const Vec3D &translation);
    ~Translation();

    void doTransformation(ITransformable &obj) override;
    void doTransformationScene(ITransformable &obj) override;
    void doTransformationPrimitive(ITransformable &obj) override;
    void doTransformationLight(ITransformable &obj) override;

    void doTranslationSphere(Sphere &sphere);
    void doTranslationPlane(Plane &plane);
    void doTranslationCylinder(Cylinder &cylinder);

  protected:
  private:
    Vec3D _translation;
};
} // namespace RayTracer
#endif /* !TRANSLATION_HPP_ */
