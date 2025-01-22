/*
** EPITECH PROJECT, 2023
** raytracer
** File description:
** NameOfTransformation.hpp
*/

#ifndef NAME_OF_TRANSFORMATION_HPP_
#define NAME_OF_TRANSFORMATION_HPP_

#include "Transformation.hpp"

namespace RayTracer
{
// Example of transformation
class NameOfTransformation : public ITransformation
{
  public:
    NameOfTransformation(void);
    ~NameOfTransformation(void);

    void doTransformation(ITransformable &obj) override;

  private:
};

} // namespace RayTracer

#endif // NAME_OF_TRANSFORMATION_HPP_
