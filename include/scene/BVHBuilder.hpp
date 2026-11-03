/*
** EPITECH PROJECT, 2024
** raytracer
** File description:
** BVHBuilder
*/

#ifndef BVH_BUILDER_HPP_
#define BVH_BUILDER_HPP_

#include "BVH.hpp"
#include "PrimitiveData.hpp"
#include <cstdint>
#include <vector>

namespace RayTracer
{

struct BVHItem
{
    std::uint32_t index;
    AABB bounds;
    Vec3D centroid;
};

struct BVHBuildResult
{
    std::vector<BVHNode> nodes;
    std::vector<std::uint32_t> indices;
    std::vector<std::uint32_t> unbounded;
};

/**
 * True when RAYTRACER_DISABLE_BVH is set to a non-zero value. Renderers fall
 * back to a linear scan, which is useful for benchmarks and parity tests.
 */
bool bvhDisabledByEnvironment(void);

/**
 * Builds a binned-SAH BVH over already-bounded items. Items are reordered in
 * place. `statsLabel` enables a stderr summary when non-null.
 *
 * Returns an empty result for tiny inputs (fewer than eight items), where a
 * linear scan is faster.
 */
BVHBuildResult buildBVHFromItems(std::vector<BVHItem> items, const char *statsLabel = nullptr);

/**
 * Builds a binned-SAH BVH over the bounded primitives. Planes and primitives
 * without bounds are returned in `unbounded` and must be tested linearly.
 */
BVHBuildResult buildBVH(const std::vector<PrimitiveData> &primitives);

} // namespace RayTracer

#endif // BVH_BUILDER_HPP_
