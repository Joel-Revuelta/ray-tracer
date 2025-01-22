/*
** EPITECH PROJECT, 2023
** raytracer
** File description:
** SceneBuilder.cpp
*/

#include "SceneBuilder.hpp"
#include "Scene.hpp"
#include <memory>

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
    for (const auto &light : sceneToAdd->getLights())
        _scene->addLight(light);
    for (const auto &primitive : sceneToAdd->getPrimitives())
        _scene->addPrimitive(primitive);
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
    return _scene;
}

/**
 * Clears the _scene variable.
 */
void SceneBuilder::clear(void)
{
    _scene.reset();
}
