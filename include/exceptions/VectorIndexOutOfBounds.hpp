/*
** EPITECH PROJECT, 2023
** raytracer
** File description:
** VectorIndexOutOfBounds.hpp
*/

#ifndef VECTOR_INDEX_OUT_OF_BOUNDS_HPP_
#define VECTOR_INDEX_OUT_OF_BOUNDS_HPP_

#include "RayTracerException.hpp"

namespace RayTracer
{
class VectorIndexOutOfBounds : public RayTracerException
{
  public:
    VectorIndexOutOfBounds(void) : RayTracerException("VectorIndexOutOfBounds: Index out of bounds\n"){};
    ~VectorIndexOutOfBounds(void){};
};
} // namespace RayTracer

#endif /* !VECTOR_INDEX_OUT_OF_BOUNDS_HPP_ */
