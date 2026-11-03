/*
** EPITECH PROJECT, 2024
** raytracer
** File description:
** ExternMesh
*/

#include "MeshPrimitive.hpp"

using namespace RayTracer;

extern "C" std::shared_ptr<IPrimitive> create(std::shared_ptr<const MeshAsset> asset, uint materialIndex,
                                              std::vector<std::pair<std::string, uint>> materialGroups)
{
    return std::make_shared<MeshPrimitive>(asset, materialIndex, std::move(materialGroups));
}
