/*
** EPITECH PROJECT, 2023
** raytracer
** File description:
** Scene.cpp
*/

#include "Scene.hpp"
#include "HitPayload.hpp"
#include "PrimitiveBounds.hpp"
#include <algorithm>
#include <cstddef>
#include <stdexcept>
#include <vector>

using namespace RayTracer;

namespace
{
struct BVHEntry
{
    std::size_t index;
    AABB bounds;
    Vec3D centroid;
};
} // namespace

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

    std::vector<BVHEntry> entries;

    entries.reserve(_primitives.size());

    for (std::size_t i = 0; i < _primitives.size(); ++i)
    {
        PrimitiveData data;

        if (!_primitives[i]->getData(data))
        {
            _unboundedPrimitives.push_back(i);
            continue;
        }

        AABB bounds;

        if (!computePrimitiveBounds(data, bounds))
        {
            _unboundedPrimitives.push_back(i);
            continue;
        }

        entries.push_back(BVHEntry{i, bounds, bounds.centroid()});
    }

    // Tiny scenes are faster with a linear scan, and unbounded primitives are
    // always tested linearly.
    if (entries.size() < 8)
    {
        _unboundedPrimitives.clear();
        return;
    }

    _bvhIndices.reserve(entries.size());
    _bvhNodes.reserve(entries.size() * 2);

    auto buildNode = [&](auto &self, std::size_t start, std::size_t end) -> std::size_t {
        const std::size_t nodeIndex = _bvhNodes.size();

        _bvhNodes.emplace_back();

        AABB bounds;

        for (std::size_t i = start; i < end; ++i)
            bounds.expand(entries[i].bounds);

        if (end - start <= 4)
        {
            BVHNode &node = _bvhNodes[nodeIndex];

            node.bounds = bounds;
            node.leaf = true;
            node.start = _bvhIndices.size();
            node.count = end - start;

            for (std::size_t i = start; i < end; ++i)
                _bvhIndices.push_back(entries[i].index);
            return nodeIndex;
        }

        const Vec3D extent = bounds.extent();
        int axis = 0;

        if (extent.getY() > extent.getX())
            axis = 1;
        if (extent.getZ() > (axis == 0 ? extent.getX() : extent.getY()))
            axis = 2;

        const std::size_t middle = (start + end) / 2;

        std::sort(entries.begin() + static_cast<std::ptrdiff_t>(start),
                  entries.begin() + static_cast<std::ptrdiff_t>(end),
                  [axis](const BVHEntry &left, const BVHEntry &right) {
                      return left.centroid[axis] < right.centroid[axis];
                  });

        const std::size_t left = self(self, start, middle);
        const std::size_t right = self(self, middle, end);

        BVHNode &node = _bvhNodes[nodeIndex];

        node.bounds = bounds;
        node.left = static_cast<int>(left);
        node.right = static_cast<int>(right);
        node.leaf = false;
        return nodeIndex;
    };

    buildNode(buildNode, 0, entries.size());
    _accelerated = true;
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

    bool hit = false;

    if (node.leaf)
    {
        for (std::size_t i = 0; i < node.count; ++i)
        {
            const std::size_t primitiveIndex = _bvhIndices[node.start + i];

            if (_primitives[primitiveIndex]->hit(ray, payload, primitiveIndex))
                hit = true;
        }
        return hit;
    }

    if (hitBVH(node.left, ray, payload))
        hit = true;
    if (hitBVH(node.right, ray, payload))
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
