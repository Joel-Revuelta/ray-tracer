/*
** EPITECH PROJECT, 2023
** raytracer
** File description:
** EmissiveMaterial.cpp
*/

#include "EmissiveMaterial.hpp"
#include <iostream>
#include <memory>

using namespace RayTracer;

EmissiveMaterial::EmissiveMaterial(const Color3D &color, const float intensity, const Color3D &emissionColor)
    : AMaterial(0.05, 0.001, 0.8, color, intensity, emissionColor, IMaterial::MaterialType::EMISSIVE)
{
}

EmissiveMaterial::~EmissiveMaterial(void)
{
}

extern "C" std::shared_ptr<IMaterial> create(const Color3D &color, const double intensity, const Color3D &emissionColor)
{
    return std::make_shared<EmissiveMaterial>(color, intensity, emissionColor);
}
