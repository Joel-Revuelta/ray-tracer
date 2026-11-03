/*
** EPITECH PROJECT, 2024
** raytracer
** File description:
** Externs
*/

#include "ExternSphere.hpp"
using namespace RayTracer;

ExternSphere::ExternSphere()
{
}

ExternSphere::~ExternSphere()
{
}

extern "C" std::shared_ptr<IPrimitive> create(double radius, Vec3D center, uint materialIdx)
{
    return std::make_shared<Sphere>(radius, center, materialIdx);
}
