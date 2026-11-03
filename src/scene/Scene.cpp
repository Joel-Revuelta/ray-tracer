/*
** EPITECH PROJECT, 2023
** raytracer
** File description:
** Scene.cpp
*/

#include "Scene.hpp"
#include "BVHBuilder.hpp"
#include "HitPayload.hpp"
#include <cstddef>
#include <stdexcept>
#include <utility>
#include <vector>

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
    for (const auto &light : _lights)
        light->doTransformation(t);

    buildAcceleration();
}

void Scene::buildAcceleration(void)
{
    _bvhNodes.clear();
    _bvhIndices.clear();
    _unboundedPrimitives.clear();
    _accelerated = false;

    std::vector<PrimitiveData> descriptions(_primitives.size());

    for (std::size_t i = 0; i < _primitives.size(); ++i)
    {
        if (!_primitives[i]->getData(descriptions[i]))
            descriptions[i].type = PrimitiveType::NONE;
    }

    BVHBuildResult result = buildBVH(descriptions);

    _bvhNodes = std::move(result.nodes);
    _bvhIndices = std::move(result.indices);
    _unboundedPrimitives = std::move(result.unbounded);
    _accelerated = !_bvhNodes.empty();
}

void Scene::clearAcceleration(void)
{
    _bvhNodes.clear();
    _bvhIndices.clear();
    _unboundedPrimitives.clear();
    _accelerated = false;
}

bool Scene::hitBVH(int nodeIndex, const Ray &ray, HitPayload &payload) const
{
    if (nodeIndex < 0)
        return false;

    const BVHNode &node = _bvhNodes[static_cast<std::size_t>(nodeIndex)];

    if (!node.bounds.intersect(ray, payload.getHitDistance()))
        return false;

    if (node.leaf)
    {
        bool hit = false;

        for (std::size_t i = 0; i < node.count; ++i)
        {
            const std::size_t primitiveIndex = _bvhIndices[node.start + i];

            if (_primitives[primitiveIndex]->hit(ray, payload, primitiveIndex))
                hit = true;
        }
        return hit;
    }

    const BVHNode &leftNode = _bvhNodes[static_cast<std::size_t>(node.left)];
    const BVHNode &rightNode = _bvhNodes[static_cast<std::size_t>(node.right)];

    // Visit the nearer child first so the far child can often be skipped.
    const float leftDistance = leftNode.bounds.entryDistance(ray);
    const float rightDistance = rightNode.bounds.entryDistance(ray);
    const int firstChild = leftDistance <= rightDistance ? node.left : node.right;
    const int secondChild = leftDistance <= rightDistance ? node.right : node.left;
    const float firstDistance = leftDistance <= rightDistance ? leftDistance : rightDistance;
    const float secondDistance = leftDistance <= rightDistance ? rightDistance : leftDistance;

    bool hit = false;

    if (firstDistance < payload.getHitDistance() && hitBVH(firstChild, ray, payload))
        hit = true;
    if (secondDistance < payload.getHitDistance() && hitBVH(secondChild, ray, payload))
        hit = true;
    return hit;
}

bool Scene::hit(const Ray &ray, HitPayload &payload, size_t i) const
{
    if (i != 0 || !_accelerated)
    {
        bool hit = false;

        for (; i < _primitives.size(); ++i)
        {
            if (_primitives[i]->hit(ray, payload, i))
                hit = true;
        }
        return hit;
    }

    bool hit = false;

    if (!_bvhNodes.empty())
        hit = hitBVH(0, ray, payload);

    for (const std::size_t primitiveIndex : _unboundedPrimitives)
    {
        if (_primitives[primitiveIndex]->hit(ray, payload, primitiveIndex))
            hit = true;
    }
    return hit;
}

void Scene::addLight(std::shared_ptr<ILight> light)
{
    if (!light)
        throw std::invalid_argument("Cannot add a null light to the scene");
    _lights.push_back(light);
}

void Scene::addPrimitive(std::shared_ptr<IPrimitive> primitive)
{
    if (!primitive)
        throw std::invalid_argument("Cannot add a null primitive to the scene");
    clearAcceleration();
    _primitives.push_back(primitive);
}

const Lights &Scene::getLights(void) const
{
    return _lights;
}

const Primitives &Scene::getPrimitives(void) const
{
    return _primitives;
}

uint Scene::getMaterialIndex(void) const
{
    return _materialIndex;
}

void Scene::setMaterialIndex(const uint &index)
{
    _materialIndex = index;
}

void Scene::addMaterial(std::shared_ptr<IMaterial> material)
{
    if (!material)
        throw std::invalid_argument("Cannot add a null material to the scene");
    _materials.push_back(material);
}

const Materials &Scene::getMaterials(void) const
{
    return _materials;
}

const Background &Scene::getBackground(void) const
{
    return _background;
}

void Scene::setBackground(const Background &background)
{
    _background = background;
}
