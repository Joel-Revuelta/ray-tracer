/*
** EPITECH PROJECT, 2023
** raytracer
** File description:
** CustomMaterial.cpp
*/

#include "CustomMaterial.hpp"
#include <iostream>
#include <memory>

using namespace RayTracer;

CustomMaterial::CustomMaterial(const float ambientReflCoeff, const float diffuseReflCoeff,
                               const float specularReflCoeff, const Color3D &diffuseColor, const float emissionPower,
                               const Color3D &emissionColor, const MaterialType type)
    : AMaterial(ambientReflCoeff, diffuseReflCoeff, specularReflCoeff, diffuseColor, emissionPower, emissionColor, type)
{
}

CustomMaterial::~CustomMaterial(void)
{
}

std::shared_ptr<IMaterial> CustomMaterial::clone(void) const
{
    return std::make_shared<CustomMaterial>(*this);
}

extern "C" std::shared_ptr<IMaterial> create(double ambientReflCoeff, double diffuseReflCoeff,
                                             double specularReflCoeff, Color3D diffuseColor, double emissionPower,
                                             Color3D emissionColor, IMaterial::MaterialType type)
{
    return std::make_shared<CustomMaterial>(ambientReflCoeff, diffuseReflCoeff, specularReflCoeff, diffuseColor,
                                            emissionPower, emissionColor, type);
}
