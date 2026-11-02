/*
** EPITECH PROJECT, 2023
** raytracer
** File description:
** SceneBuilder.cpp
*/

#include "SceneBuilder.hpp"
#include "Scene.hpp"
#include <memory>
#include <stdexcept>

using namespace RayTracer;

SceneBuilder::SceneBuilder(void) : _scene(std::make_shared<Scene>())
{
}

SceneBuilder::~SceneBuilder(void)
{
}

/**
 * @brief Adds a light to _scene.
 *
 * This function adds a light to the scene being built.
 *
 * @param light The light to be added to the scene.
 */
void SceneBuilder::addLight(std::shared_ptr<ILight> light)
{
    _scene->addLight(light);
}

void SceneBuilder::addMaterial(std::shared_ptr<IMaterial> material)
{
    _scene->addMaterial(material);
}

/**
 * @brief Adds a primitive to _scene.
 *
 * This function adds the specified primitive to the scene being built.
 *
 * @param primitive The primitive to be added to the scene.
 */
void SceneBuilder::addPrimitive(std::shared_ptr<IPrimitive> primitive)
{
    _scene->addPrimitive(primitive);
}

/**
 * @brief Adds a scene to _scene.
 *
 * This function adds the specified scene to the scene being built.
 *
 * @param sceneToAdd The scene to add.
 */
void SceneBuilder::addScene(std::shared_ptr<Scene> sceneToAdd)
{
    if (!sceneToAdd)
        return;

    // Materials of a nested scene are appended to the parent scene, so every
    // primitive coming from that scene has to shift its material index.
    const uint materialOffset = static_cast<uint>(_scene->getMaterials().size());

    for (const auto &material : sceneToAdd->getMaterials())
        _scene->addMaterial(material);
    for (const auto &primitive : sceneToAdd->getPrimitives())
    {
        primitive->setMaterialIndex(primitive->getMaterialIndex() + materialOffset);
        _scene->addPrimitive(primitive);
    }
    for (const auto &light : sceneToAdd->getLights())
        _scene->addLight(light);
}

/**
 * @brief Retuns a Scene object.
 *
 * This function returns the _scene variable from the builder.
 *
 * @return The constructed Scene object.
 */
std::shared_ptr<Scene> SceneBuilder::build(void)
{
    const Materials &materials = _scene->getMaterials();

    for (const auto &primitive : _scene->getPrimitives())
    {
        if (primitive->getMaterialIndex() >= materials.size())
            throw std::runtime_error("A primitive references a material index that does not exist");
    }
    return _scene;
}

/**
 * Clears the _scene variable.
 */
void SceneBuilder::clear(void)
{
    _scene.reset();
}
