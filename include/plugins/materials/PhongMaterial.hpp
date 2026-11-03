/*
** EPITECH PROJECT, 2024
** raytracer
** File description:
** PhongMaterial
*/

#ifndef PHONG_MATERIAL_HPP_
#define PHONG_MATERIAL_HPP_

#include "AMaterial.hpp"

namespace RayTracer
{

/**
 * Diffuse material with an explicit Phong specular term and roughness derived
 * from an MTL Ns value.
 */
class PhongMaterial : public AMaterial
{
  public:
    PhongMaterial(float ambientReflection, float diffuseReflection, float specularReflection,
                  const Color3D &diffuseColor, float roughness);
    ~PhongMaterial(void) override;

    float getRoughness(void) const override;
    std::shared_ptr<IMaterial> clone(void) const override;

  private:
    float _roughness;
};

} // namespace RayTracer

#endif // PHONG_MATERIAL_HPP_
