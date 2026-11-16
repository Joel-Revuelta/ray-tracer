/*
** EPITECH PROJECT, 2023
** raytracer
** File description:
** Utils.cpp
*/

#include "Utils.hpp"
#include "Color.hpp"

using namespace RayTracer;

/**
 * PCGHash function.
 * This function takes an input value and returns a hashed value using the PCG
 * algorithm.
 *
 * @param input The input value to be hashed.
 * @return The hashed value.
 */
uint32_t Utils::PCGHash(uint32_t input)
{
    uint32_t state = input * 747796405u + 2891336453u;
    uint32_t word = ((state >> ((state >> 28u) + 4u)) ^ state) * 277803737u;
    return (word >> 22u) ^ word;
}

/**
 * Converts a Color4D object to an RGBA value.
 *
 * @param color The Color4D object to convert.
 * @return The RGBA value representing the color.
 */
uint32_t Utils::convertToRGBA(const Color4D &color)
{
    return convertToRGBA(color, PostProcessSettings{});
}

uint32_t Utils::convertToRGBA(const Color4D &color, const PostProcessSettings &settings)
{
    const uint8_t r = postProcessToByte(color.getX(), settings);
    const uint8_t g = postProcessToByte(color.getY(), settings);
    const uint8_t b = postProcessToByte(color.getZ(), settings);
    const uint8_t a = postProcessToByte(color.getW(), settings, true);

    return (static_cast<uint32_t>(a) << 24u) | (static_cast<uint32_t>(b) << 16u) |
           (static_cast<uint32_t>(g) << 8u) | static_cast<uint32_t>(r);
}
