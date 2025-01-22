/*
** EPITECH PROJECT, 2023
** raytracer
** File description:
** DiffuseMaterial.hpp
*/

#ifndef DIFFUSE_MATERIAL_HPP_
#define DIFFUSE_MATERIAL_HPP_

#include "AMaterial.hpp"

namespace RayTracer
{
class DiffuseMaterial : public AMaterial
{
  public:
    DiffuseMaterial(const float &ambientReflectionCoefficient, const float &diffuseReflectionCoefficient,
                    const Color3D &color);
    ~DiffuseMaterial(void);
};
} // namespace RayTracer

#endif // METALLIC_MATERIAL_HPP_
