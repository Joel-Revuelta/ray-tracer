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
#include <vector>

namespace RayTracer
{

using Lights = std::vector<std::shared_ptr<ILight>>;
using Primitives = std::vector<std::shared_ptr<IPrimitive>>;
using Materials = std::vector<std::shared_ptr<IMaterial>>;

class Scene : IPrimitive
{
  public:
    Scene(void);
    ~Scene(void);

    void doTransformation(const ITransformation &t) override;
    bool hit(const Ray &ray, HitPayload &record, size_t i) const override;

    void addLight(std::shared_ptr<ILight> light);
    void addMaterial(std::shared_ptr<IMaterial> material);
    void addPrimitive(std::shared_ptr<IPrimitive> primitive);

    Lights getLights(void) const;
    Materials getMaterials(void) const;
    Primitives getPrimitives(void) const;

    uint getMaterialIndex(void) const override;
    void setMaterialIndex(const uint &index) override;

  private:
    Lights _lights;
    Materials _materials;
    Primitives _primitives;
    uint _materialIndex = -1;
};

} // namespace RayTracer

#endif // SCENE_HPP_
