/*
** EPITECH PROJECT, 2023
** raytracer
** File description:
** ITransformation.hpp
*/

#ifndef TRANSFORMATION_HPP_
#define TRANSFORMATION_HPP_

namespace RayTracer
{

class ITransformable;

class ITransformation // Interface
{
  public:
    virtual ~ITransformation(void) = default;

    // Apply this transformation to an object. Transformations do not mutate
    // their own state, so they can be applied through a const reference.
    virtual void doTransformation(ITransformable &obj) const = 0;

  private:
    virtual void doTransformationPrimitive(ITransformable &obj) const = 0;
    virtual void doTransformationScene(ITransformable &obj) const = 0;
    virtual void doTransformationLight(ITransformable &obj) const = 0;
};
} // namespace RayTracer

#endif // TRANSFORMATION_HPP_
