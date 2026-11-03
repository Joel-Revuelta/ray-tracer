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

void SceneBuilder::ensureScene(void)
{
    if (!_scene)
        _scene = std::make_shared<Scene>();
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
    if (!light)
        return;

    ensureScene();
    _scene->addLight(light);
}

void SceneBuilder::addMaterial(std::shared_ptr<IMaterial> material)
{
    if (!material)
        return;

    ensureScene();
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
    if (!primitive)
        return;

    ensureScene();
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

    ensureScene();

    if (sceneToAdd.get() == _scene.get())
        throw std::invalid_argument("Cannot merge a scene into itself");

    // Materials of a nested scene are appended to the parent scene, so every
    // primitive coming from that scene has to shift its material index.
    const uint materialOffset = static_cast<uint>(_scene->getMaterials().size());

    for (const auto &material : sceneToAdd->getMaterials())
    {
        std::shared_ptr<IMaterial> copy = material->clone();

        if (!copy)
            copy = material;

        _scene->addMaterial(copy);
    }
    for (const auto &primitive : sceneToAdd->getPrimitives())
    {
        std::shared_ptr<IPrimitive> copy = primitive->clone();

        // Fall back to sharing for primitive types that do not implement clone.
        if (!copy)
            copy = primitive;

        copy->setMaterialIndex(copy->getMaterialIndex() + materialOffset);
        _scene->addPrimitive(copy);
    }
    for (const auto &light : sceneToAdd->getLights())
    {
        std::shared_ptr<ILight> copy = light->clone();

        if (!copy)
            copy = light;

        _scene->addLight(copy);
    }
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
    ensureScene();

    const Materials &materials = _scene->getMaterials();

    for (const auto &primitive : _scene->getPrimitives())
    {
        if (primitive->getMaterialIndex() >= materials.size())
            throw std::runtime_error("A primitive references a material index that does not exist");
    }

    _scene->buildAcceleration();
    return _scene;
}

/**
 * Clears the _scene variable.
 */
void SceneBuilder::clear(void)
{
    _scene = std::make_shared<Scene>();
}
