/*
** EPITECH PROJECT, 2023
** raytracer
** File description:
** IMaterial.hpp
*/

#ifndef I_MATERIAL_HPP_
#define I_MATERIAL_HPP_

#include "Color.hpp"

namespace RayTracer
{

class IMaterial
{
  public:
    enum MaterialType
    {
        DIFFUSE,
        FLAT_COLOR,
        EMISSIVE,
        METALLIC,
        DIELECTRIC, // Not implemented
        TYPE_COUNT
    };
    virtual ~IMaterial(void) = default;

    virtual MaterialType getType(void) const = 0;
    virtual Color3D getDiffuseColor(void) const = 0;
    virtual Color3D getEmissionColor(void) const = 0;
    virtual float getAmbientReflectionCoefficient(void) const = 0;
    virtual float getDiffuseReflectionCoefficient(void) const = 0;
    virtual float getSpecularReflectionCoefficient(void) const = 0;
    virtual float getEmissionPower(void) const = 0;

    virtual void setType(const MaterialType &type) = 0;
    virtual void setDiffuseColor(const Color3D &diffuseColor) = 0;
    virtual void setEmissionPower(const float &emissionPower) = 0;
    virtual void setEmissionColor(const Color3D &emissionColor) = 0;
    virtual void setAmbientReflectionCoefficient(const float &ambientReflectionCoefficient) = 0;
    virtual void setDiffuseReflectionCoefficient(const float &diffuseReflectionCoefficient) = 0;
    virtual void setSpecularReflectionCoefficient(const float &specularReflectionCoefficient) = 0;
};

} // namespace RayTracer

#endif // I_MATERIAL_HPP_
