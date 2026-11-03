/*
** EPITECH PROJECT, 2024
** raytracer
** File description:
** BVHBuilder
*/

#include "BVHBuilder.hpp"
#include "PrimitiveBounds.hpp"
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <limits>

using namespace RayTracer;

namespace
{
constexpr int BIN_COUNT = 16;
constexpr std::size_t LEAF_SIZE = 4;
constexpr std::size_t MINIMUM_BVH_SIZE = 8;
constexpr int MAX_SAH_DEPTH = 48;
constexpr float CENTROID_EPSILON = 1e-9f;

struct BVHEntry
{
    std::uint32_t index;
    AABB bounds;
    Vec3D centroid;
};

struct Bin
{
    AABB bounds;
    std::size_t count = 0;
};

int chooseSplitAxis(const AABB &bounds)
{
    const Vec3D extent = bounds.extent();
    int axis = 0;

    if (extent.getY() > extent.getX())
        axis = 1;
    if (extent.getZ() > (axis == 0 ? extent.getX() : extent.getY()))
        axis = 2;
    return axis;
}

int binIndex(float centroid, float minimum, float extent)
{
    if (extent <= CENTROID_EPSILON)
        return 0;

    const int index = static_cast<int>((centroid - minimum) / extent * static_cast<float>(BIN_COUNT));

    return std::clamp(index, 0, BIN_COUNT - 1);
}

std::size_t medianSplit(std::vector<BVHEntry> &entries, std::size_t start, std::size_t end)
{
    AABB centroidBounds;

    for (std::size_t i = start; i < end; ++i)
        centroidBounds.expand(entries[i].centroid);

    const int axis = chooseSplitAxis(centroidBounds);

    const std::size_t middle = start + (end - start) / 2;

    std::nth_element(entries.begin() + static_cast<std::ptrdiff_t>(start),
                     entries.begin() + static_cast<std::ptrdiff_t>(middle),
                     entries.begin() + static_cast<std::ptrdiff_t>(end),
                     [axis](const BVHEntry &left, const BVHEntry &right) {
                         return left.centroid[axis] < right.centroid[axis];
                     });
    return middle;
}
} // namespace

bool RayTracer::bvhDisabledByEnvironment(void)
{
    const char *value = std::getenv("RAYTRACER_DISABLE_BVH");

    return value != nullptr && *value != '\0' && std::strcmp(value, "0") != 0;
}

BVHBuildResult RayTracer::buildBVH(const std::vector<PrimitiveData> &primitives)
{
    BVHBuildResult result;

    if (bvhDisabledByEnvironment())
        return result;

    const auto buildStart = std::chrono::steady_clock::now();
    std::vector<BVHEntry> entries;

    entries.reserve(primitives.size());

    for (std::size_t i = 0; i < primitives.size(); ++i)
    {
        AABB bounds;

        if (!computePrimitiveBounds(primitives[i], bounds))
        {
            result.unbounded.push_back(static_cast<std::uint32_t>(i));
            continue;
        }

        entries.push_back(BVHEntry{static_cast<std::uint32_t>(i), bounds, bounds.centroid()});
    }

    // Tiny scenes are faster with a linear scan, and unbounded primitives are
    // always tested linearly.
    if (entries.size() < MINIMUM_BVH_SIZE)
    {
        result.unbounded.clear();
        return result;
    }

    result.indices.reserve(entries.size());
    result.nodes.reserve(entries.size() * 2);

    auto buildNode = [&](auto &self, std::size_t start, std::size_t end, int depth) -> std::size_t {
        const std::size_t nodeIndex = result.nodes.size();

        result.nodes.emplace_back();

        AABB bounds;

        for (std::size_t i = start; i < end; ++i)
            bounds.expand(entries[i].bounds);

        if (end - start <= LEAF_SIZE)
        {
            BVHNode &node = result.nodes[nodeIndex];

            node.bounds = bounds;
            node.leaf = true;
            node.start = result.indices.size();
            node.count = end - start;

            for (std::size_t i = start; i < end; ++i)
                result.indices.push_back(entries[i].index);
            return nodeIndex;
        }

        AABB centroidBounds;

        for (std::size_t i = start; i < end; ++i)
            centroidBounds.expand(entries[i].centroid);

        float bestCost = std::numeric_limits<float>::infinity();
        int bestAxis = -1;
        int bestBin = -1;

        for (int axis = 0; depth < MAX_SAH_DEPTH && axis < 3; ++axis)
        {
            const float minimum = centroidBounds.min[axis];
            const float extent = centroidBounds.max[axis] - minimum;

            if (extent <= CENTROID_EPSILON)
                continue;

            Bin bins[BIN_COUNT];

            for (std::size_t i = start; i < end; ++i)
            {
                const int index = binIndex(entries[i].centroid[axis], minimum, extent);

                bins[index].bounds.expand(entries[i].bounds);
                bins[index].count++;
            }

            float leftArea[BIN_COUNT] = {0.0f};
            std::size_t leftCount[BIN_COUNT] = {0};
            AABB leftBounds;

            for (int bin = 0; bin < BIN_COUNT - 1; ++bin)
            {
                leftBounds.expand(bins[bin].bounds);
                leftArea[bin] = leftBounds.surfaceArea();
                leftCount[bin] = (bin == 0 ? 0 : leftCount[bin - 1]) + bins[bin].count;
            }

            AABB rightBounds;
            std::size_t rightCount = 0;

            for (int bin = BIN_COUNT - 1; bin > 0; --bin)
            {
                rightBounds.expand(bins[bin].bounds);
                rightCount += bins[bin].count;

                const float cost = leftArea[bin - 1] * static_cast<float>(leftCount[bin - 1]) +
                                   rightBounds.surfaceArea() * static_cast<float>(rightCount);

                if (cost < bestCost)
                {
                    bestCost = cost;
                    bestAxis = axis;
                    bestBin = bin - 1;
                }
            }
        }

        std::size_t middle;

        if (bestAxis < 0)
        {
            middle = medianSplit(entries, start, end);
        }
        else
        {
            const float minimum = centroidBounds.min[bestAxis];
            const float extent = centroidBounds.max[bestAxis] - minimum;
            const std::size_t splitBin = static_cast<std::size_t>(bestBin);

            const auto split = std::partition(entries.begin() + static_cast<std::ptrdiff_t>(start),
                                              entries.begin() + static_cast<std::ptrdiff_t>(end),
                                              [bestAxis, minimum, extent, splitBin](const BVHEntry &entry) {
                                                  return static_cast<std::size_t>(binIndex(entry.centroid[bestAxis],
                                                                                           minimum, extent)) <= splitBin;
                                              });

            middle = static_cast<std::size_t>(split - entries.begin());
        }

        // Degenerate centroid distributions can collapse a SAH split to one
        // side. Fall back to the median so the tree always makes progress.
        if (middle == start || middle == end)
            middle = medianSplit(entries, start, end);

        const std::size_t left = self(self, start, middle, depth + 1);
        const std::size_t right = self(self, middle, end, depth + 1);

        BVHNode &node = result.nodes[nodeIndex];

        node.bounds = bounds;
        node.left = static_cast<int>(left);
        node.right = static_cast<int>(right);
        node.leaf = false;
        return nodeIndex;
    };

    buildNode(buildNode, 0, entries.size(), 0);

    const char *stats = std::getenv("RAYTRACER_BVH_STATS");

    if (stats != nullptr && *stats != '\0' && std::strcmp(stats, "0") != 0)
    {
        const double elapsed =
            std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - buildStart).count();

        std::cerr << "BVH: " << entries.size() << " bounded, " << result.unbounded.size() << " unbounded, "
                  << result.nodes.size() << " nodes, " << elapsed << " ms" << std::endl;
    }

    return result;
}
