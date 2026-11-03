/*
** EPITECH PROJECT, 2024
** raytracer
** File description:
** Mesh
*/

#ifndef MESH_HPP_
#define MESH_HPP_

#include "AABB.hpp"
#include "BVHBuilder.hpp"
#include "Vec3D.hpp"
#include <vector>

namespace RayTracer
{

/**
 * A single triangle in the mesh's local space.
 */
struct Triangle
{
    Vec3D v0;
    Vec3D v1;
    Vec3D v2;
};

/**
 * Immutable mesh geometry shared by every instance of the same asset.
 * `bvh` is built in local space; instances apply their own transform.
 */
struct MeshAsset
{
    std::vector<Triangle> triangles;
    AABB bounds;
    BVHBuildResult bvh;
};

} // namespace RayTracer

#endif // MESH_HPP_
