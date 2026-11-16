/*
** EPITECH PROJECT, 2024
** raytracer
** File description:
** Transform
*/

#ifndef TRANSFORM_HPP_
#define TRANSFORM_HPP_

#include "AABB.hpp"
#include "TransformData.hpp"
#include "Vec3D.hpp"

namespace RayTracer
{

/** Human-readable transform components used by scene animation. */
struct TransformPose
{
    Vec3D translation;
    Vec3D rotation; // Euler degrees, applied in X, then Y, then Z order.
    Vec3D scale{1.0f, 1.0f, 1.0f};
};

struct Quaternion
{
    float w = 1.0f;
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
};

/**
 * Builds a local-to-world transform from translation, Euler rotation and
 * scale. Rotations are in degrees and applied in X, then Y, then Z order, so
 * the resulting linear part is Rz * Ry * Rx * S.
 */
TransformData makeTransform(const Vec3D &translation, const Vec3D &rotationDegrees, const Vec3D &scale);
TransformData makeTransform(const TransformPose &pose);

/**
 * Builds a transform from a quaternion rotation. The quaternion is normalized
 * internally and must represent the same Rz * Ry * Rx convention as the Euler
 * version.
 */
TransformData makeTransform(const Vec3D &translation, const Quaternion &rotation, const Vec3D &scale);

Quaternion quaternionFromEulerDegrees(const Vec3D &rotationDegrees);
Quaternion slerp(const Quaternion &first, const Quaternion &second, float amount);

/**
 * Composes two transforms: the result applies `child` first, then `parent`.
 */
TransformData composeTransforms(const TransformData &parent, const TransformData &child);

bool isIdentityTransform(const TransformData &transform);
bool isInvertibleTransform(const TransformData &transform);

Vec3D transformPoint(const TransformData &transform, const Vec3D &point);
Vec3D transformVector(const TransformData &transform, const Vec3D &vector);
Vec3D transformInversePoint(const TransformData &transform, const Vec3D &point);
Vec3D transformInverseVector(const TransformData &transform, const Vec3D &vector);
Vec3D transformNormal(const TransformData &transform, const Vec3D &normal);

/**
 * Conservative uniform scale of the transform: the maximum length of its
 * three basis columns. Used to scale spherical lights under non-uniform
 * transforms.
 */
float transformScaleFactor(const TransformData &transform);

/**
 * Returns the world-space bounding box of a local-space box.
 */
AABB transformBounds(const TransformData &transform, const AABB &localBounds);

} // namespace RayTracer

#endif // TRANSFORM_HPP_
