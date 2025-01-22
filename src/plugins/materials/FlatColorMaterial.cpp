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

extern "C" std::shared_ptr<IMaterial> create(const Color3D &color)
{
    return std::make_shared<FlatColorMaterial>(color);
}
