/*
** EPITECH PROJECT, 2023
** raytracer
** File description:
** MetallicMaterial.hpp
*/

#ifndef METALLIC_MATERIAL_HPP_
#define METALLIC_MATERIAL_HPP_

#include "AMaterial.hpp"

namespace RayTracer
{
class MetallicMaterial : public AMaterial
{
  public:
    MetallicMaterial(const Color3D &color, float roughness = 0.0f);
    ~MetallicMaterial(void);
    float getRoughness(void) const override;
    std::shared_ptr<IMaterial> clone(void) const override;

  private:
    float _roughness;
};
} // namespace RayTracer

#endif // METALLIC_MATERIAL_HPP_
