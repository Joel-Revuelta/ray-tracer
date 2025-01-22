/*
** EPITECH PROJECT, 2023
** raytracer
** File description:
** CustomMaterial.hpp
*/

#ifndef CUSTOM_MATERIAL_HPP_
#define CUSTOM_MATERIAL_HPP_

#include "AMaterial.hpp"

namespace RayTracer
{
class CustomMaterial : public AMaterial
{
  public:
    CustomMaterial(const float ambientReflCoeff, const float diffuseReflCoeff, const float specularReflCoeff,
                   const Color3D &diffuseColor, const float emissionPower, const Color3D &emissionColor,
                   const MaterialType type);
    ~CustomMaterial(void);
};
} // namespace RayTracer

#endif // CUSTOM_MATERIAL_HPP_
