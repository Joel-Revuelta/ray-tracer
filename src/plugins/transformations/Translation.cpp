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

void Translation::doTransformation(ITransformable &obj) const
{
    if (dynamic_cast<Scene *>(&obj))
        doTransformationScene(obj);
    else if (dynamic_cast<IPrimitive *>(&obj))
        doTransformationPrimitive(obj);
    else if (dynamic_cast<ILight *>(&obj))
        doTransformationLight(obj);
}

void Translation::doTransformationScene(ITransformable &obj) const
{
    Scene *scene = dynamic_cast<Scene *>(&obj);

    for (const auto &primitive : scene->getPrimitives())
        doTransformationPrimitive(*primitive);
    for (const auto &light : scene->getLights())
        doTransformationLight(*light);
}

void Translation::doTransformationPrimitive(ITransformable &obj) const
{
    IPrimitive *primitive = dynamic_cast<IPrimitive *>(&obj);

    if (dynamic_cast<Sphere *>(primitive))
        doTranslationSphere(*dynamic_cast<Sphere *>(primitive));
    else if (dynamic_cast<Plane *>(primitive))
        doTranslationPlane(*dynamic_cast<Plane *>(primitive));
    else if (dynamic_cast<Cylinder *>(primitive))
        doTranslationCylinder(*dynamic_cast<Cylinder *>(primitive));
    else if (dynamic_cast<Cone *>(primitive))
        doTranslationCone(*dynamic_cast<Cone *>(primitive));
}

void Translation::doTranslationSphere(Sphere &sphere) const
{
    sphere.setCenter(sphere.getCenter() + _translation);
}

void Translation::doTranslationPlane(Plane &plane) const
{
    plane.setPoint(plane.getPoint() + _translation);
}

void Translation::doTranslationCylinder(Cylinder &cylinder) const
{
    cylinder.setCenter(cylinder.getCenter() + _translation);
}

void Translation::doTranslationCone(Cone &cone) const
{
    cone.setCenter(cone.getCenter() + _translation);
}

void Translation::doTransformationLight(ITransformable &obj) const
{
    PointLight *pointLight = dynamic_cast<PointLight *>(&obj);

    if (pointLight)
        pointLight->setPosition(pointLight->getPosition() + _translation);
}

extern "C" std::shared_ptr<ITransformation> create(Vec3D translation)
{
    return std::make_shared<Translation>(translation);
}
