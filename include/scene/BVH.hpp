/*
** EPITECH PROJECT, 2024
** raytracer
** File description:
** BVH
*/

#ifndef BVH_HPP_
#define BVH_HPP_

#include "AABB.hpp"
#include <cstddef>

namespace RayTracer
{

struct BVHNode
{
    AABB bounds;
    int left = -1;
    int right = -1;
    std::size_t start = 0;
    std::size_t count = 0;
    bool leaf = true;
};

} // namespace RayTracer

#endif // BVH_HPP_
