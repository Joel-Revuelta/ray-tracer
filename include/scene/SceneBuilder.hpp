/*
** EPITECH PROJECT, 2023
** raytracer
** File description:
** SceneBuilder.hpp
*/

#ifndef SCENE_BUILDER_HPP_
#define SCENE_BUILDER_HPP_

#include "IMaterial.hpp"
#include "Light.hpp"
#include "Primitive.hpp"
#include "Scene.hpp"
#include "TransformData.hpp"
#include <cstddef>
#include <memory>

namespace RayTracer
{

class SceneBuilder
{
  public:
    SceneBuilder(void);
    ~SceneBuilder(void);

    void clear(void);
    std::shared_ptr<Scene> build(void);

    void addLight(std::shared_ptr<ILight> light);
    void addMaterial(std::shared_ptr<IMaterial> material);
    void addPrimitive(std::shared_ptr<IPrimitive> primitive);
    void addScene(std::shared_ptr<Scene> sceneToAdd, const TransformData &transform = TransformData{});
    void setBackground(const Background &background);

    std::size_t getMaterialCount(void) const;

  private:
    void ensureScene(void);

    std::shared_ptr<Scene> _scene;
};

} // namespace RayTracer

#endif // SCENE_BUILDER_HPP_
