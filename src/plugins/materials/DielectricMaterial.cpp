/*
** EPITECH PROJECT, 2024
** raytracer
** File description:
** DielectricMaterial
*/

#include "DielectricMaterial.hpp"
#include <memory>

using namespace RayTracer;

DielectricMaterial::DielectricMaterial(const Color3D &color, float ior)
    : AMaterial(0.0f, 0.0f, 1.0f, color, 0.0f, Color3D(0.0f, 0.0f, 0.0f),
                IMaterial::MaterialType::DIELECTRIC),
      _ior(ior)
{
}

DielectricMaterial::~DielectricMaterial(void)
{
}

float DielectricMaterial::getIndexOfRefraction(void) const
{
    return _ior;
}

std::shared_ptr<IMaterial> DielectricMaterial::clone(void) const
{
    return std::make_shared<DielectricMaterial>(*this);
}

extern "C" std::shared_ptr<IMaterial> create(Color3D color, double ior)
{
    return std::make_shared<DielectricMaterial>(color, static_cast<float>(ior));
}
