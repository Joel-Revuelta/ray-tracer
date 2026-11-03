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
    Vec3D n0;
    Vec3D n1;
    Vec3D n2;
    float tu0 = 0.0f;
    float tv0 = 0.0f;
    float tu1 = 0.0f;
    float tv1 = 0.0f;
    float tu2 = 0.0f;
    float tv2 = 0.0f;
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
