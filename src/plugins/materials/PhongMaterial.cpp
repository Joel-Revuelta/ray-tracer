/*
** EPITECH PROJECT, 2024
** raytracer
** File description:
** PhongMaterial
*/

#include "PhongMaterial.hpp"
#include <memory>

using namespace RayTracer;

PhongMaterial::PhongMaterial(float ambientReflection, float diffuseReflection, float specularReflection,
                             const Color3D &diffuseColor, float roughness)
    : AMaterial(ambientReflection, diffuseReflection, specularReflection, diffuseColor, 0.0f,
                Color3D(0.0f, 0.0f, 0.0f), IMaterial::DIFFUSE),
      _roughness(roughness)
{
}

PhongMaterial::~PhongMaterial(void)
{
}

float PhongMaterial::getRoughness(void) const
{
    return _roughness;
}

std::shared_ptr<IMaterial> PhongMaterial::clone(void) const
{
    return std::make_shared<PhongMaterial>(*this);
}

extern "C" std::shared_ptr<IMaterial> create(double ambientReflection, double diffuseReflection,
                                             double specularReflection, Color3D diffuseColor, double roughness)
{
    return std::make_shared<PhongMaterial>(static_cast<float>(ambientReflection),
                                           static_cast<float>(diffuseReflection),
                                           static_cast<float>(specularReflection), diffuseColor,
                                           static_cast<float>(roughness));
}
