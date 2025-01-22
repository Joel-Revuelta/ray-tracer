/*
** EPITECH PROJECT, 2024
** raytracer
** File description:
** translation
*/

#include "Translation.hpp"

using namespace RayTracer;

Translation::Translation() : _translation(Vec3D(0, 0, 0))
{
}

Translation::Translation(const Vec3D &translation) : _translation(translation)
{
}

Translation::~Translation()
{
}

void Translation::doTransformation(ITransformable &obj)
{
    if (dynamic_cast<Scene *>(&obj))
        doTransformationScene(obj);
    else if (dynamic_cast<IPrimitive *>(&obj))
        doTransformationPrimitive(obj);
}

void Translation::doTransformationScene(ITransformable &obj)
{
    Scene *scene = dynamic_cast<Scene *>(&obj);

    for (auto &primitive : scene->getPrimitives())
        doTransformationPrimitive(*primitive);
}

void Translation::doTransformationPrimitive(ITransformable &obj)
{
    IPrimitive *primitive = dynamic_cast<IPrimitive *>(&obj);

    if (dynamic_cast<Sphere *>(primitive))
        doTranslationSphere(*dynamic_cast<Sphere *>(primitive));
    else if (dynamic_cast<Plane *>(primitive))
        doTranslationPlane(*dynamic_cast<Plane *>(primitive));
    else if (dynamic_cast<Cylinder *>(primitive))
        doTranslationCylinder(*dynamic_cast<Cylinder *>(primitive));
}

void Translation::doTranslationSphere(Sphere &sphere)
{
    sphere.setCenter(sphere.getCenter() + _translation);
}

void Translation::doTranslationPlane(Plane &plane)
{
    plane.setPoint(plane.getPoint() + _translation);
}

void Translation::doTranslationCylinder(Cylinder &cylinder)
{
    cylinder.setCenter(cylinder.getCenter() + _translation);
}

void Translation::doTransformationLight(ITransformable &obj)
{
    (void)obj;
}

extern "C" std::shared_ptr<ITransformation> create(const Vec3D &translation)
{
    return std::make_shared<Translation>(translation);
}
