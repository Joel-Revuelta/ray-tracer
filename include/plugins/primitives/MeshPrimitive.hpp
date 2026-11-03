/*
** EPITECH PROJECT, 2024
** raytracer
** File description:
** MeshPrimitive
*/

#ifndef MESH_PRIMITIVE_HPP_
#define MESH_PRIMITIVE_HPP_

#include "Mesh.hpp"
#include "Primitive.hpp"
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace RayTracer
{

/**
 * Triangle mesh primitive. The asset is immutable and shared between
 * instances; transforms are applied by InstancePrimitive.
 */
class MeshPrimitive : public IPrimitive
{
  public:
    MeshPrimitive(std::shared_ptr<const MeshAsset> asset, uint materialIndex,
                  std::vector<std::pair<std::string, uint>> materialGroups = {});

    void doTransformation(const ITransformation &t) override;
    bool hit(const Ray &ray, HitPayload &payload, size_t i) const override;
    std::shared_ptr<IPrimitive> clone(void) const override;
    bool getData(PrimitiveData &data) const override;
    std::shared_ptr<const MeshAsset> getMeshAsset(void) const override;
    std::shared_ptr<const std::vector<int>> getMeshMaterialMap(void) const override;
    void shiftMaterialIndices(uint offset) override;

    uint getMaterialIndex(void) const override;
    void setMaterialIndex(const uint &index) override;

  private:
    bool hitBVH(int nodeIndex, const Ray &ray, HitPayload &payload, size_t i) const;
    bool hitTriangle(const Triangle &triangle, const Ray &ray, HitPayload &payload, size_t i) const;

    std::shared_ptr<const MeshAsset> _asset;
    uint _materialIndex;
    std::shared_ptr<const std::vector<int>> _materialMap;
};

} // namespace RayTracer

#endif // MESH_PRIMITIVE_HPP_
