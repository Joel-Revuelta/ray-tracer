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

extern "C" std::shared_ptr<IPrimitive> create(Vec3D normal, Vec3D point, uint _materialIndex)
{
    return std::make_shared<Plane>(normal, point, _materialIndex);
}
