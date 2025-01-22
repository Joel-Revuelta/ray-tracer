/*
** EPITECH PROJECT, 2023
** raytracer
** File description:
** DiffuseMaterial.cpp
*/

#include "DiffuseMaterial.hpp"
#include <iostream>
#include <memory>

using namespace RayTracer;

DiffuseMaterial::DiffuseMaterial(const float &ambientReflectionCoefficient, const float &diffuseReflectionCoefficient,
                                 const Color3D &color)
    : AMaterial(ambientReflectionCoefficient, diffuseReflectionCoefficient, 0.001, color, 0.0, Color3D(0.0, 0.0, 0.0),
                IMaterial::MaterialType::DIFFUSE)
{
}

DiffuseMaterial::~DiffuseMaterial(void)
{
}

extern "C" std::shared_ptr<IMaterial> create(const double ambientReflectionCoefficient,
                                             const double diffuseReflectionCoefficient, const Color3D &color)
{
    return std::make_shared<DiffuseMaterial>(ambientReflectionCoefficient, diffuseReflectionCoefficient, color);
}
