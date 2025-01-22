/*
** EPITECH PROJECT, 2023
** raytracer
** File description:
** MetallicMaterial.cpp
*/

#include "MetallicMaterial.hpp"
#include <iostream>
#include <memory>

using namespace RayTracer;

MetallicMaterial::MetallicMaterial(const Color3D &color)
    : AMaterial(0.05, 0.001, 0.8, color, 0.0, Color3D(0.0, 0.0, 0.0), IMaterial::MaterialType::METALLIC)
{
}

MetallicMaterial::~MetallicMaterial(void)
{
}

extern "C" std::shared_ptr<IMaterial> create(const Color3D &color)
{
    return std::make_shared<MetallicMaterial>(color);
}
