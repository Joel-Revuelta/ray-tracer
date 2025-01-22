/*
** EPITECH PROJECT, 2023
** raytracer
** File description:
** FlatColorMaterial.hpp
*/

#ifndef FLATCOLOR_MATERIAL_HPP_
#define FLATCOLOR_MATERIAL_HPP_

#include "AMaterial.hpp"

namespace RayTracer
{
class FlatColorMaterial : public AMaterial
{
  public:
    FlatColorMaterial(const Color3D &color);
    ~FlatColorMaterial(void);
};
} // namespace RayTracer

#endif // FLATCOLOR_MATERIAL_HPP_
