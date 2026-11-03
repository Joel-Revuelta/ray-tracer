/*
** EPITECH PROJECT, 2024
** raytracer
** File description:
** DielectricMaterial
*/

#ifndef DIELECTRIC_MATERIAL_HPP_
#define DIELECTRIC_MATERIAL_HPP_

#include "AMaterial.hpp"

namespace RayTracer
{

class DielectricMaterial : public AMaterial
{
  public:
    DielectricMaterial(const Color3D &color, float ior);
    ~DielectricMaterial(void);

    float getIndexOfRefraction(void) const override;
    std::shared_ptr<IMaterial> clone(void) const override;

  private:
    float _ior;
};

} // namespace RayTracer

#endif // DIELECTRIC_MATERIAL_HPP_
