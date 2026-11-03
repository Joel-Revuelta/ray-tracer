/*
** EPITECH PROJECT, 2024
** raytarcer
** File description:
** ExternCone
*/

#include "ExternCone.hpp"

using namespace RayTracer;

ExternCone::ExternCone()
{
}

ExternCone::~ExternCone()
{
}

extern "C" std::shared_ptr<IPrimitive> create(double radius, Vec3D center, double height, uint material)
{
    return std::make_shared<Cone>(radius, center, height, material);
}
