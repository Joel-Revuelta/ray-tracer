/*
** EPITECH PROJECT, 2024
** raytracer
** File description:
** ExternPlane
*/

#include "ExternPlane.hpp"
using namespace RayTracer;

ExternPlane::ExternPlane()
{
}

ExternPlane::~ExternPlane()
{
}

extern "C" std::shared_ptr<IPrimitive> create(const Vec3D &normal, const Point3D &point, uint _materialIndex)
{
    return std::make_shared<Plane>(normal, point, _materialIndex);
}
