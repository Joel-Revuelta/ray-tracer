/*
** EPITECH PROJECT, 2023
** raytracer
** File description:
** IMaterial.hpp
*/

#ifndef I_MATERIAL_HPP_
#define I_MATERIAL_HPP_

#include "Color.hpp"
#include <memory>

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

    // Refractive index used by dielectric materials. Non-dielectric materials
    // inherit this default and never use it.
    virtual float getIndexOfRefraction(void) const
    {
        return 1.5f;
    }

    // Metallic microsurface roughness in [0, 1]. Non-metallic materials use
    // the default and ignore it.
    virtual float getRoughness(void) const
    {
        return 0.0f;
    }

    // Returns an independent copy for scene composition. Materials that do not
    // implement cloning fall back to sharing the original instance.
    virtual std::shared_ptr<IMaterial> clone(void) const
    {
        return nullptr;
    }

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
