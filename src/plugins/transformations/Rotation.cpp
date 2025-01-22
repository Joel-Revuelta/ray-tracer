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

void Rotation::doTransformation(ITransformable &obj)
{
    if (dynamic_cast<Scene *>(&obj))
        doTransformationScene(obj);
    else if (dynamic_cast<IPrimitive *>(&obj))
        doTransformationPrimitive(obj);
}

void Rotation::doTransformationScene(ITransformable &obj)
{
    Scene *scene = dynamic_cast<Scene *>(&obj);

    for (auto &primitive : scene->getPrimitives())
        doTransformationPrimitive(*primitive);
}

void Rotation::doTransformationPrimitive(ITransformable &obj)
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

void Rotation::doTransformationLight(ITransformable &obj)
{
    (void)obj;
}

void Rotation::doRotationSphere(Sphere &sphere)
{
    (void)sphere;
}

void Rotation::doRotationCylinder(Cylinder &cylinder)
{
    cylinder.setAxis(Vec3D(_rotation.getX(), _rotation.getY(), _rotation.getZ()));
}

void Rotation::doRotationCone(Cone &cone)
{
    cone.setRotation(true);
}

void Rotation::doRotationPlane(Plane &plane)
{
    plane.setNormal(_rotation);
}

extern "C" std::shared_ptr<ITransformation> create(const Vec3D &rotation)
{
    return std::make_shared<Rotation>(rotation);
}
