/*
** EPITECH PROJECT, 2023
** raytracer
** File description:
** IPrimitive.hpp
*/

#ifndef PRIMITIVE_HPP_
#define PRIMITIVE_HPP_

#include "HitPayload.hpp"
#include "Ray.hpp"
#include "Transformable.hpp"

namespace RayTracer
{

class IPrimitive : public ITransformable
{
  public:
    virtual ~IPrimitive(void) = default;

    virtual bool hit(const Ray &ray, HitPayload &payload, size_t i) const = 0;

    virtual uint getMaterialIndex(void) const = 0;

    virtual void setMaterialIndex(const uint &index) = 0;
};

} // namespace RayTracer

#endif // PRIMITIVE_HPP_
