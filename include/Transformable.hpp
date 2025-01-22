/*
** EPITECH PROJECT, 2023
** raytracer
** File description:
** ITransformable.hpp
*/

#ifndef TRANSFORMABLE_HPP_
#define TRANSFORMABLE_HPP_

#include "Transformation.hpp"

namespace RayTracer
{

// Enables 'thing' to handle transoformations
class ITransformable
{
  public:
    virtual ~ITransformable(void) = default;

    // Inside of this function we will call t.doTransformation(*this)
    virtual void doTransformation(const ITransformation &t) = 0;
};

} // namespace RayTracer

#endif // TRANSFORMABLE_HPP_
