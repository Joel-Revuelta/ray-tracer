/*
** EPITECH PROJECT, 2023
** raytracer
** File description:
** AMaterial.cpp
*/

#include "AMaterial.hpp"
#include <memory>

using namespace RayTracer;

AMaterial::AMaterial(const float ambientReflCoeff, const float diffuseReflCoeff, const float specularReflCoeff,
                     const Color3D &diffuseColor, const float emissionPower, const Color3D &emissionColor,
                     const MaterialType type)
    : _type(type), _emissionPower(emissionPower), _diffuseColor(diffuseColor), _emissionColor(emissionColor),
      _ambientReflectionCoefficient(ambientReflCoeff), _diffuseReflectionCoefficient(diffuseReflCoeff),
      _specularReflectionCoefficient(specularReflCoeff)
{
}

AMaterial::~AMaterial(void)
{
}

Color3D AMaterial::getDiffuseColor(void) const
{
    return _diffuseColor;
}

float AMaterial::getEmissionPower(void) const
{
    return _emissionPower;
}

Color3D AMaterial::getEmissionColor(void) const
{
    return _emissionColor * getEmissionPower();
}

AMaterial::MaterialType AMaterial::getType(void) const
{
    return _type;
}

float AMaterial::getAmbientReflectionCoefficient(void) const
{
    return _ambientReflectionCoefficient;
}

float AMaterial::getDiffuseReflectionCoefficient(void) const
{
    return _diffuseReflectionCoefficient;
}

float AMaterial::getSpecularReflectionCoefficient(void) const
{
    return _specularReflectionCoefficient;
}

void AMaterial::setDiffuseColor(const Color3D &diffuseColor)
{
    _diffuseColor = diffuseColor;
};

void AMaterial::setEmissionPower(const float &emissionPower)
{
    _emissionPower = emissionPower;
}

void AMaterial::setEmissionColor(const Color3D &emissionColor)
{
    _emissionColor = emissionColor;
}

void AMaterial::setType(const MaterialType &type)
{
    _type = type;
}

void AMaterial::setAmbientReflectionCoefficient(const float &ambientReflectionCoefficient)
{
    _ambientReflectionCoefficient = ambientReflectionCoefficient;
}

void AMaterial::setDiffuseReflectionCoefficient(const float &diffuseReflectionCoefficient)
{
    _diffuseReflectionCoefficient = diffuseReflectionCoefficient;
}

void AMaterial::setSpecularReflectionCoefficient(const float &specularReflectionCoefficient)
{
    _specularReflectionCoefficient = specularReflectionCoefficient;
}
