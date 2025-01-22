/*
** EPITECH PROJECT, 2024
** B-OOP-400-BAR-4-1-raytracer-matheus.lopez
** File description:
** ExternCylinder
*/

#include "ExternCylinder.hpp"
using namespace RayTracer;

ExternCylinder::ExternCylinder()
{
}

ExternCylinder::~ExternCylinder()
{
}

extern "C" std::shared_ptr<IPrimitive> create(const double radius, const Vec3D &center, const double height,
                                              const uint material)
{
    return std::make_shared<Cylinder>(radius, center, height, material);
}
