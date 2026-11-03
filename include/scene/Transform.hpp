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

/**
 * Builds a local-to-world transform from translation, Euler rotation and
 * scale. Rotations are in degrees and applied in X, then Y, then Z order, so
 * the resulting linear part is Rz * Ry * Rx * S.
 */
TransformData makeTransform(const Vec3D &translation, const Vec3D &rotationDegrees, const Vec3D &scale);

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
 * Returns the world-space bounding box of a local-space box.
 */
AABB transformBounds(const TransformData &transform, const AABB &localBounds);

} // namespace RayTracer

#endif // TRANSFORM_HPP_
