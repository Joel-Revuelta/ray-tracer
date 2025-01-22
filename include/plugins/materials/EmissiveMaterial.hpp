/*
** EPITECH PROJECT, 2023
** raytracer
** File description:
** EmissiveMaterial.hpp
*/

#ifndef EMISSIVE_MATERIAL_HPP_
#define EMISSIVE_MATERIAL_HPP_

#include "AMaterial.hpp"

namespace RayTracer
{
class EmissiveMaterial : public AMaterial
{
  public:
    EmissiveMaterial(const Color3D &color, const float intensity, const Color3D &emissionColor);
    ~EmissiveMaterial(void);
};
} // namespace RayTracer

#endif // EMISSIVE_MATERIAL_HPP_
