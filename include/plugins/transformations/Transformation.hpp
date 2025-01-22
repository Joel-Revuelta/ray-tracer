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

    // recive object and detect type and depending of type do necessary operations, Use typeid and dynamic_cast
    virtual void doTransformation(ITransformable &obj) = 0;

  private:
    virtual void doTransformationPrimitive(ITransformable &obj) = 0;
    virtual void doTransformationScene(ITransformable &obj) = 0;
    virtual void doTransformationLight(ITransformable &obj) = 0;
};
} // namespace RayTracer

#endif // TRANSFORMATION_HPP_
