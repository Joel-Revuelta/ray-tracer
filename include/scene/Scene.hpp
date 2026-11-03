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
#include "Light.hpp"
#include "Primitive.hpp"
#include "BVH.hpp"
#include <cstddef>
#include <limits>
#include <vector>

namespace RayTracer
{

using Lights = std::vector<std::shared_ptr<ILight>>;
using Primitives = std::vector<std::shared_ptr<IPrimitive>>;
using Materials = std::vector<std::shared_ptr<IMaterial>>;

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

    const Lights &getLights(void) const;
    const Materials &getMaterials(void) const;
    const Primitives &getPrimitives(void) const;

    uint getMaterialIndex(void) const override;
    void setMaterialIndex(const uint &index) override;

  private:
    void clearAcceleration(void);
    bool hitBVH(int nodeIndex, const Ray &ray, HitPayload &record) const;

    Lights _lights;
    Materials _materials;
    Primitives _primitives;
    std::vector<BVHNode> _bvhNodes;
    std::vector<std::size_t> _bvhIndices;
    std::vector<std::size_t> _unboundedPrimitives;
    bool _accelerated = false;
    uint _materialIndex = std::numeric_limits<uint>::max();
};

} // namespace RayTracer

#endif // SCENE_HPP_
