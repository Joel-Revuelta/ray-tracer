/*
** EPITECH PROJECT, 2023
** raytracer
** File description:
** Scene.cpp
*/

#include "Scene.hpp"
#include "HitPayload.hpp"
#include <cstddef>

using namespace RayTracer;

Scene::Scene(void)
{
}

Scene::~Scene(void)
{
}

void Scene::doTransformation(const ITransformation &t)
{
    for (const auto &primitive : _primitives)
        primitive->doTransformation(t);
}

bool Scene::hit(const Ray &ray, HitPayload &payload, size_t i) const
{
    bool hit = false;

    for (; i < getPrimitives().size(); ++i)
    {
        if (getPrimitives()[i]->hit(ray, payload, i))
            hit = true;
    }
    return hit;
}

void Scene::addLight(std::shared_ptr<ILight> light)
{
    _lights.push_back(light);
}

void Scene::addPrimitive(std::shared_ptr<IPrimitive> primitive)
{
    _primitives.push_back(primitive);
}

Lights Scene::getLights(void) const
{
    return _lights;
}

Primitives Scene::getPrimitives(void) const
{
    return _primitives;
}

uint Scene::getMaterialIndex(void) const
{
    return 0;
}

void Scene::setMaterialIndex(const uint &index)
{
    _materialIndex = index;
}

void Scene::addMaterial(std::shared_ptr<IMaterial> material)
{
    _materials.push_back(material);
}

Materials Scene::getMaterials(void) const
{
    return _materials;
}
