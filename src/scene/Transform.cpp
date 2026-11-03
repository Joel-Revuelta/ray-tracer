/*
** EPITECH PROJECT, 2024
** raytracer
** File description:
** Transform
*/

#include "Transform.hpp"
#include <cmath>

using namespace RayTracer;

namespace
{
constexpr float IDENTITY_EPSILON = 1e-6f;
constexpr float DETERMINANT_EPSILON = 1e-18f;
constexpr float PI = 3.14159265358979323846f;

float degreesToRadians(float degrees)
{
    return degrees * PI / 180.0f;
}

void multiplyMatrix(const float left[9], const float right[9], float result[9])
{
    for (int row = 0; row < 3; ++row)
    {
        for (int column = 0; column < 3; ++column)
        {
            result[row * 3 + column] = left[row * 3 + 0] * right[0 * 3 + column] +
                                       left[row * 3 + 1] * right[1 * 3 + column] +
                                       left[row * 3 + 2] * right[2 * 3 + column];
        }
    }
}

Vec3D multiplyMatrix(const float matrix[9], const Vec3D &vector)
{
    return Vec3D(matrix[0] * vector.getX() + matrix[1] * vector.getY() + matrix[2] * vector.getZ(),
                 matrix[3] * vector.getX() + matrix[4] * vector.getY() + matrix[5] * vector.getZ(),
                 matrix[6] * vector.getX() + matrix[7] * vector.getY() + matrix[8] * vector.getZ());
}

bool invertMatrix(const float matrix[9], float inverse[9])
{
    const float a = matrix[0];
    const float b = matrix[1];
    const float c = matrix[2];
    const float d = matrix[3];
    const float e = matrix[4];
    const float f = matrix[5];
    const float g = matrix[6];
    const float h = matrix[7];
    const float i = matrix[8];

    const float determinant = a * (e * i - f * h) - b * (d * i - f * g) + c * (d * h - e * g);

    if (!std::isfinite(determinant) || std::fabs(determinant) < DETERMINANT_EPSILON)
        return false;

    const float scale = 1.0f / determinant;

    inverse[0] = (e * i - f * h) * scale;
    inverse[1] = (c * h - b * i) * scale;
    inverse[2] = (b * f - c * e) * scale;
    inverse[3] = (f * g - d * i) * scale;
    inverse[4] = (a * i - c * g) * scale;
    inverse[5] = (c * d - a * f) * scale;
    inverse[6] = (d * h - e * g) * scale;
    inverse[7] = (b * g - a * h) * scale;
    inverse[8] = (a * e - b * d) * scale;
    return true;
}

void updateInverse(TransformData &transform)
{
    if (!invertMatrix(transform.linear, transform.inverseLinear))
    {
        for (int i = 0; i < 9; ++i)
            transform.inverseLinear[i] = 0.0f;
        for (int i = 0; i < 3; ++i)
            transform.inverseTranslation[i] = 0.0f;
        return;
    }

    const Vec3D translation(transform.translation[0], transform.translation[1], transform.translation[2]);
    const Vec3D inverseTranslation = multiplyMatrix(transform.inverseLinear, translation);

    transform.inverseTranslation[0] = -inverseTranslation.getX();
    transform.inverseTranslation[1] = -inverseTranslation.getY();
    transform.inverseTranslation[2] = -inverseTranslation.getZ();
}

void rotationMatrix(const Vec3D &rotationDegrees, float result[9])
{
    const float cx = std::cos(degreesToRadians(rotationDegrees.getX()));
    const float sx = std::sin(degreesToRadians(rotationDegrees.getX()));
    const float cy = std::cos(degreesToRadians(rotationDegrees.getY()));
    const float sy = std::sin(degreesToRadians(rotationDegrees.getY()));
    const float cz = std::cos(degreesToRadians(rotationDegrees.getZ()));
    const float sz = std::sin(degreesToRadians(rotationDegrees.getZ()));

    const float rotationX[9] = {1.0f, 0.0f, 0.0f, 0.0f, cx, -sx, 0.0f, sx, cx};
    const float rotationY[9] = {cy, 0.0f, sy, 0.0f, 1.0f, 0.0f, -sy, 0.0f, cy};
    const float rotationZ[9] = {cz, -sz, 0.0f, sz, cz, 0.0f, 0.0f, 0.0f, 1.0f};
    float temporary[9];

    multiplyMatrix(rotationZ, rotationY, temporary);
    multiplyMatrix(temporary, rotationX, result);
}
} // namespace

TransformData RayTracer::makeTransform(const Vec3D &translation, const Vec3D &rotationDegrees, const Vec3D &scale)
{
    TransformData transform;
    float rotation[9];

    rotationMatrix(rotationDegrees, rotation);

    for (int row = 0; row < 3; ++row)
    {
        transform.linear[row * 3 + 0] = rotation[row * 3 + 0] * scale.getX();
        transform.linear[row * 3 + 1] = rotation[row * 3 + 1] * scale.getY();
        transform.linear[row * 3 + 2] = rotation[row * 3 + 2] * scale.getZ();
    }

    transform.translation[0] = translation.getX();
    transform.translation[1] = translation.getY();
    transform.translation[2] = translation.getZ();
    updateInverse(transform);
    return transform;
}

TransformData RayTracer::composeTransforms(const TransformData &parent, const TransformData &child)
{
    TransformData result;

    multiplyMatrix(parent.linear, child.linear, result.linear);

    const Vec3D childTranslation(child.translation[0], child.translation[1], child.translation[2]);
    const Vec3D rotatedTranslation = multiplyMatrix(parent.linear, childTranslation);

    result.translation[0] = rotatedTranslation.getX() + parent.translation[0];
    result.translation[1] = rotatedTranslation.getY() + parent.translation[1];
    result.translation[2] = rotatedTranslation.getZ() + parent.translation[2];
    updateInverse(result);
    return result;
}

bool RayTracer::isIdentityTransform(const TransformData &transform)
{
    const float identity[9] = {1.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 1.0f};

    for (int i = 0; i < 9; ++i)
    {
        if (std::fabs(transform.linear[i] - identity[i]) > IDENTITY_EPSILON)
            return false;
    }

    for (int i = 0; i < 3; ++i)
    {
        if (std::fabs(transform.translation[i]) > IDENTITY_EPSILON)
            return false;
    }

    return true;
}

bool RayTracer::isInvertibleTransform(const TransformData &transform)
{
    float inverse[9];

    return invertMatrix(transform.linear, inverse);
}

Vec3D RayTracer::transformPoint(const TransformData &transform, const Vec3D &point)
{
    const Vec3D rotated = multiplyMatrix(transform.linear, point);

    return rotated + Vec3D(transform.translation[0], transform.translation[1], transform.translation[2]);
}

Vec3D RayTracer::transformVector(const TransformData &transform, const Vec3D &vector)
{
    return multiplyMatrix(transform.linear, vector);
}

Vec3D RayTracer::transformInversePoint(const TransformData &transform, const Vec3D &point)
{
    const Vec3D rotated = multiplyMatrix(transform.inverseLinear, point);

    return rotated +
           Vec3D(transform.inverseTranslation[0], transform.inverseTranslation[1], transform.inverseTranslation[2]);
}

Vec3D RayTracer::transformInverseVector(const TransformData &transform, const Vec3D &vector)
{
    return multiplyMatrix(transform.inverseLinear, vector);
}

Vec3D RayTracer::transformNormal(const TransformData &transform, const Vec3D &normal)
{
    // The inverse-transpose is the transpose of the inverse linear part.
    return Vec3D(transform.inverseLinear[0] * normal.getX() + transform.inverseLinear[3] * normal.getY() +
                     transform.inverseLinear[6] * normal.getZ(),
                 transform.inverseLinear[1] * normal.getX() + transform.inverseLinear[4] * normal.getY() +
                     transform.inverseLinear[7] * normal.getZ(),
                 transform.inverseLinear[2] * normal.getX() + transform.inverseLinear[5] * normal.getY() +
                     transform.inverseLinear[8] * normal.getZ());
}

float RayTracer::transformScaleFactor(const TransformData &transform)
{
    const float x = std::sqrt(transform.linear[0] * transform.linear[0] +
                              transform.linear[3] * transform.linear[3] +
                              transform.linear[6] * transform.linear[6]);
    const float y = std::sqrt(transform.linear[1] * transform.linear[1] +
                              transform.linear[4] * transform.linear[4] +
                              transform.linear[7] * transform.linear[7]);
    const float z = std::sqrt(transform.linear[2] * transform.linear[2] +
                              transform.linear[5] * transform.linear[5] +
                              transform.linear[8] * transform.linear[8]);

    return std::max(x, std::max(y, z));
}

AABB RayTracer::transformBounds(const TransformData &transform, const AABB &localBounds)
{
    AABB result;

    if (!localBounds.valid())
        return result;

    if (isIdentityTransform(transform))
        return localBounds;

    for (int corner = 0; corner < 8; ++corner)
    {
        const Vec3D point((corner & 1) ? localBounds.max.getX() : localBounds.min.getX(),
                          (corner & 2) ? localBounds.max.getY() : localBounds.min.getY(),
                          (corner & 4) ? localBounds.max.getZ() : localBounds.min.getZ());

        result.expand(transformPoint(transform, point));
    }

    return result;
}
