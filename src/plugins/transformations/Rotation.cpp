/*
** EPITECH PROJECT, 2024
** raytracer
** File description:
** Rotation
*/

#include "Rotation.hpp"

using namespace RayTracer;

Rotation::Rotation()
{
}

Rotation::Rotation(Vec3D rotation) : _rotation(rotation)
{
}

Rotation::~Rotation()
{
}

void Rotation::doTransformation(ITransformable &obj) const
{
    if (dynamic_cast<Scene *>(&obj))
        doTransformationScene(obj);
    else if (dynamic_cast<IPrimitive *>(&obj))
        doTransformationPrimitive(obj);
    else if (dynamic_cast<ILight *>(&obj))
        doTransformationLight(obj);
}

void Rotation::doTransformationScene(ITransformable &obj) const
{
    Scene *scene = dynamic_cast<Scene *>(&obj);

    for (const auto &primitive : scene->getPrimitives())
        doTransformationPrimitive(*primitive);
    for (const auto &light : scene->getLights())
        doTransformationLight(*light);
}

void Rotation::doTransformationPrimitive(ITransformable &obj) const
{
    IPrimitive *primitive = dynamic_cast<IPrimitive *>(&obj);

    if (dynamic_cast<Sphere *>(primitive))
    {
        Sphere *sphere = dynamic_cast<Sphere *>(primitive);
        doRotationSphere(*sphere);
    }
    else if (dynamic_cast<Cylinder *>(primitive))
    {
        Cylinder *cylinder = dynamic_cast<Cylinder *>(primitive);
        doRotationCylinder(*cylinder);
    }
    else if (dynamic_cast<Cone *>(primitive))
    {
        Cone *cone = dynamic_cast<Cone *>(primitive);
        doRotationCone(*cone);
    }
    else if (dynamic_cast<Plane *>(primitive))
    {
        Plane *plane = dynamic_cast<Plane *>(primitive);
        doRotationPlane(*plane);
    }
}

void Rotation::doTransformationLight(ITransformable &obj) const
{
    (void)obj;
}

void Rotation::doRotationSphere(Sphere &sphere) const
{
    (void)sphere;
}

void Rotation::doRotationCylinder(Cylinder &cylinder) const
{
    cylinder.setAxis(_rotation);
}

void Rotation::doRotationCone(Cone &cone) const
{
    cone.setRotation(true);
}

void Rotation::doRotationPlane(Plane &plane) const
{
    plane.setNormal(_rotation);
}

extern "C" std::shared_ptr<ITransformation> create(Vec3D rotation)
{
    return std::make_shared<Rotation>(rotation);
}
