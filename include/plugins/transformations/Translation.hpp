/*
** EPITECH PROJECT, 2024
** raytracer
** File description:
** Translation.hpp
*/

#ifndef TRANSLATION_HPP_
#define TRANSLATION_HPP_

#include "Cone.hpp"
#include "Cylinder.hpp"
#include "Light.hpp"
#include "Plane.hpp"
#include "PointLight.hpp"
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

    void doTransformation(ITransformable &obj) const override;
    void doTransformationScene(ITransformable &obj) const override;
    void doTransformationPrimitive(ITransformable &obj) const override;
    void doTransformationLight(ITransformable &obj) const override;

    void doTranslationSphere(Sphere &sphere) const;
    void doTranslationPlane(Plane &plane) const;
    void doTranslationCylinder(Cylinder &cylinder) const;
    void doTranslationCone(Cone &cone) const;

  protected:
  private:
    Vec3D _translation;
};
} // namespace RayTracer
#endif /* !TRANSLATION_HPP_ */
