/*
** EPITECH PROJECT, 2023
** raytracer
** File description:
** FlatColorMaterial.cpp
*/

#include "FlatColorMaterial.hpp"
#include <memory>

using namespace RayTracer;

FlatColorMaterial::FlatColorMaterial(const Color3D &color)
    : AMaterial(1.0, 1.0, 0.0, color, 0.0, Color3D(0.0, 0.0, 0.0), IMaterial::MaterialType::FLAT_COLOR)
{
}

FlatColorMaterial::~FlatColorMaterial(void)
{
}

std::shared_ptr<IMaterial> FlatColorMaterial::clone(void) const
{
    return std::make_shared<FlatColorMaterial>(*this);
}

extern "C" std::shared_ptr<IMaterial> create(Color3D color)
{
    return std::make_shared<FlatColorMaterial>(color);
}
