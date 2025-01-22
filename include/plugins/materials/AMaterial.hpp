/*
** EPITECH PROJECT, 2023
** raytracer
** File description:
** AMaterial.hpp
*/

#ifndef A_MATERIAL_HPP_
#define A_MATERIAL_HPP_

#include "Color.hpp"
#include "IMaterial.hpp"

namespace RayTracer
{

class AMaterial : public IMaterial
{
  public:
    AMaterial(const float ambientReflCoeff, const float diffuseReflCoeff, const float specularReflCoeff,
              const Color3D &diffuseColor, const float emissionPower, const Color3D &emissionColor,
              const MaterialType type);
    ~AMaterial(void);

    MaterialType getType(void) const override;
    Color3D getDiffuseColor(void) const override;
    Color3D getEmissionColor(void) const override;
    float getAmbientReflectionCoefficient(void) const override;
    float getDiffuseReflectionCoefficient(void) const override;
    float getSpecularReflectionCoefficient(void) const override;
    float getEmissionPower(void) const override;

    void setType(const MaterialType &type) override;
    void setDiffuseColor(const Color3D &diffuseColor) override;
    void setEmissionPower(const float &emissionPower) override;
    void setEmissionColor(const Color3D &emissionColor) override;
    void setAmbientReflectionCoefficient(const float &ambientReflectionCoefficient) override;
    void setDiffuseReflectionCoefficient(const float &diffuseReflectionCoefficient) override;
    void setSpecularReflectionCoefficient(const float &specularReflectionCoefficient) override;

  protected:
    // Specifies the type of material (e.g., DIFFUSE, EMISSIVE, REFLECTIVE)
    MaterialType _type;

    // Specifies the power of the emission for emissive materials
    float _emissionPower;

    // Specifies the color of the material (e.g., for diffuse reflection)
    Color3D _diffuseColor;

    // Specifies the color emitted by the material for emissive materials
    Color3D _emissionColor;

    // Coefficients for various reflection components

    // These coefficients control how much each type of reflection contributes to the final color
    // Range: Usally between 0.0 and 1.0
    // Values closer to 0.0 result in less reflection of that type,
    float _ambientReflectionCoefficient;
    float _diffuseReflectionCoefficient;
    float _specularReflectionCoefficient;
};

} // namespace RayTracer

#endif // A_MATERIAL_HPP_
