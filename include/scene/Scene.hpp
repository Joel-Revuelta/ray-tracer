/*
** EPITECH PROJECT, 2023
** raytracer
** File description:
** Scene.hpp
*/

#ifndef SCENE_HPP_
#define SCENE_HPP_

#include "Color.hpp"
#include "IMaterial.hpp"
#include "InstancePrimitive.hpp"
#include "Light.hpp"
#include "Primitive.hpp"
#include "Background.hpp"
#include "BVH.hpp"
#include <cstddef>
#include <cstdint>
#include <limits>
#include <string>
#include <unordered_map>
#include <vector>

namespace RayTracer
{

using Lights = std::vector<std::shared_ptr<ILight>>;
using Primitives = std::vector<std::shared_ptr<IPrimitive>>;
using Materials = std::vector<std::shared_ptr<IMaterial>>;
using InstanceGroup = std::vector<std::shared_ptr<InstancePrimitive>>;

class Scene : public IPrimitive
{
  public:
    Scene(void);
    ~Scene(void);

    void doTransformation(const ITransformation &t) override;
    bool hit(const Ray &ray, HitPayload &record, size_t i) const override;

    // Builds the CPU acceleration structure. SceneBuilder calls this after all
    // primitives and material indices have been validated.
    void buildAcceleration(void);

    void addLight(std::shared_ptr<ILight> light);
    void addMaterial(std::shared_ptr<IMaterial> material);
    void addPrimitive(std::shared_ptr<IPrimitive> primitive);
    void addInstanceGroup(const std::string &id, const InstanceGroup &instances);
    void associateLight(const std::string &id, std::shared_ptr<ILight> light);

    const Lights &getLights(void) const;
    const Materials &getMaterials(void) const;
    const Primitives &getPrimitives(void) const;
    const Background &getBackground(void) const;
    void setBackground(const Background &background);
    const std::unordered_map<std::string, InstanceGroup> &getInstanceGroups(void) const;
    const InstanceGroup *findInstanceGroup(const std::string &id) const;
    const std::unordered_map<std::string, std::shared_ptr<ILight>> &getLightsById(void) const;
    std::shared_ptr<ILight> findLight(const std::string &id) const;

    uint getMaterialIndex(void) const override;
    void setMaterialIndex(const uint &index) override;

  private:
    void clearAcceleration(void);
    bool hitBVH(int nodeIndex, const Ray &ray, HitPayload &record) const;

    Lights _lights;
    Materials _materials;
    Primitives _primitives;
    std::unordered_map<std::string, InstanceGroup> _instanceGroups;
    std::unordered_map<std::string, std::shared_ptr<ILight>> _lightsById;
    Background _background;
    std::vector<BVHNode> _bvhNodes;
    std::vector<std::uint32_t> _bvhIndices;
    std::vector<std::uint32_t> _unboundedPrimitives;
    bool _accelerated = false;
    uint _materialIndex = std::numeric_limits<uint>::max();
};

} // namespace RayTracer

#endif // SCENE_HPP_
