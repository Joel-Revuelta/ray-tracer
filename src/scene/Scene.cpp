/*
** EPITECH PROJECT, 2023
** raytracer
** File description:
** Scene.cpp
*/

#include "Scene.hpp"
#include "BVHBuilder.hpp"
#include "PrimitiveBounds.hpp"
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

bool Scene::refitAcceleration(void)
{
    if (_bvhNodes.empty())
        return true;

    if (_bvhIndices.size() > _primitives.size())
        return false;

    std::vector<PrimitiveData> descriptions(_primitives.size());
    std::vector<AABB> primitiveBounds(_primitives.size());
    std::vector<bool> bounded(_primitives.size(), false);

    for (std::size_t i = 0; i < _primitives.size(); ++i)
    {
        if (!_primitives[i]->getData(descriptions[i]))
            descriptions[i].type = PrimitiveType::NONE;
        bounded[i] = computePrimitiveBounds(descriptions[i], primitiveBounds[i]);
    }

    std::vector<bool> indexed(_primitives.size(), false);
    std::vector<bool> unbounded(_primitives.size(), false);

    for (const std::uint32_t index : _bvhIndices)
    {
        if (index >= _primitives.size())
            return false;
        indexed[index] = true;
    }

    for (const std::uint32_t index : _unboundedPrimitives)
    {
        if (index >= _primitives.size())
            return false;
        unbounded[index] = true;
    }

    for (std::size_t i = 0; i < _primitives.size(); ++i)
    {
        if (bounded[i])
        {
            if (!indexed[i] || unbounded[i])
                return false;
        }
        else if (!unbounded[i] || indexed[i])
            return false;
    }

    // Compute a post-order list without recursion so deep trees cannot overflow
    // the stack. Children always appear after their parent in `order`, so the
    // reversed pass updates leaves before internal nodes.
    std::vector<int> order;
    std::vector<int> pending;

    order.reserve(_bvhNodes.size());
    pending.push_back(0);

    while (!pending.empty())
    {
        const int nodeIndex = pending.back();

        pending.pop_back();

        if (nodeIndex < 0 || static_cast<std::size_t>(nodeIndex) >= _bvhNodes.size())
            return false;

        order.push_back(nodeIndex);

        const BVHNode &node = _bvhNodes[static_cast<std::size_t>(nodeIndex)];

        if (!node.leaf)
        {
            if (node.left < 0 || node.right < 0)
                return false;
            pending.push_back(node.left);
            pending.push_back(node.right);
        }
    }

    if (order.size() != _bvhNodes.size())
        return false;

    for (auto iterator = order.rbegin(); iterator != order.rend(); ++iterator)
    {
        BVHNode &node = _bvhNodes[static_cast<std::size_t>(*iterator)];
        AABB bounds;

        if (node.leaf)
        {
            if (node.start > _bvhIndices.size() || node.count > _bvhIndices.size() - node.start)
                return false;

            for (std::size_t i = 0; i < node.count; ++i)
            {
                const std::uint32_t primitiveIndex = _bvhIndices[node.start + i];

                if (primitiveIndex >= primitiveBounds.size() || !bounded[primitiveIndex])
                    return false;
                bounds.expand(primitiveBounds[primitiveIndex]);
            }
        }
        else
        {
            if (node.left < 0 || node.right < 0 || static_cast<std::size_t>(node.left) >= _bvhNodes.size() ||
                static_cast<std::size_t>(node.right) >= _bvhNodes.size())
                return false;
            bounds.expand(_bvhNodes[static_cast<std::size_t>(node.left)].bounds);
            bounds.expand(_bvhNodes[static_cast<std::size_t>(node.right)].bounds);
        }

        if (!bounds.valid())
            return false;
        node.bounds = bounds;
    }

    return true;
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

void Scene::addInstanceGroup(const std::string &id, const InstanceGroup &instances)
{
    if (id.empty())
        throw std::invalid_argument("Instance group id must not be empty");
    if (instances.empty())
        throw std::invalid_argument("Instance group must contain at least one primitive");
    if (_instanceGroups.find(id) != _instanceGroups.end())
        throw std::runtime_error("Duplicate instance id: " + id);

    _instanceGroups.emplace(id, instances);
}

void Scene::associateLight(const std::string &id, std::shared_ptr<ILight> light)
{
    if (id.empty())
        throw std::invalid_argument("Light id must not be empty");
    if (!light)
        throw std::invalid_argument("Cannot associate a null light");
    if (_lightsById.find(id) != _lightsById.end())
        throw std::runtime_error("Duplicate light id: " + id);

    _lightsById.emplace(id, std::move(light));
}

const Lights &Scene::getLights(void) const
{
    return _lights;
}

const std::unordered_map<std::string, InstanceGroup> &Scene::getInstanceGroups(void) const
{
    return _instanceGroups;
}

const InstanceGroup *Scene::findInstanceGroup(const std::string &id) const
{
    const auto found = _instanceGroups.find(id);

    return found == _instanceGroups.end() ? nullptr : &found->second;
}

const std::unordered_map<std::string, std::shared_ptr<ILight>> &Scene::getLightsById(void) const
{
    return _lightsById;
}

std::shared_ptr<ILight> Scene::findLight(const std::string &id) const
{
    const auto found = _lightsById.find(id);

    return found == _lightsById.end() ? nullptr : found->second;
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

const Atmosphere &Scene::getAtmosphere(void) const
{
    return _atmosphere;
}

void Scene::setAtmosphere(const Atmosphere &atmosphere)
{
    _atmosphere = atmosphere;
}
