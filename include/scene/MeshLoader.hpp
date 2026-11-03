/*
** EPITECH PROJECT, 2024
** raytracer
** File description:
** MeshLoader
*/

#ifndef MESH_LOADER_HPP_
#define MESH_LOADER_HPP_

#include "Mesh.hpp"
#include <memory>
#include <string>

namespace RayTracer
{

/**
 * Loads a Wavefront OBJ file into an immutable mesh asset.
 *
 * Supports `v` positions and `f` faces (triangles and polygons, fan
 * triangulated). `vt`, `vn`, groups, objects and material references are
 * ignored. Throws std::runtime_error with the file and line on invalid input.
 */
std::shared_ptr<const MeshAsset> loadObjMesh(const std::string &path);

} // namespace RayTracer

#endif // MESH_LOADER_HPP_
