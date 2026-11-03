/*
** EPITECH PROJECT, 2024
** raytracer
** File description:
** TransformData
*/

#ifndef TRANSFORM_DATA_HPP_
#define TRANSFORM_DATA_HPP_

namespace RayTracer
{

/**
 * Device-friendly affine transform.
 *
 * The linear part is stored row-major (row * 3 + column). The inverse fields
 * are precomputed so both renderers can transform rays without decomposing the
 * matrix again. A default-constructed value is the identity transform.
 */
struct TransformData
{
    float linear[9]{1.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 1.0f};
    float translation[3]{0.0f, 0.0f, 0.0f};
    float inverseLinear[9]{1.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 1.0f};
    float inverseTranslation[3]{0.0f, 0.0f, 0.0f};
};

} // namespace RayTracer

#endif // TRANSFORM_DATA_HPP_
