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
    uint8_t r = (uint8_t)(color.getX() * 255.0f);
    uint8_t g = (uint8_t)(color.getY() * 255.0f);
    uint8_t b = (uint8_t)(color.getZ() * 255.0f);
    uint8_t a = (uint8_t)(color.getW() * 255.0f);

    uint32_t result = (a << 24) | (b << 16) | (g << 8) | r;
    return result;
}

/**
 * Converts a 32-bit RGBA color value to a Color4D object.
 *
 * @param rgba The 32-bit RGBA color value to convert.
 * @return The Color4D object representing the converted color.
 */
Color4D Utils::convertFromRGBA(const uint32_t &rgba)
{
    float r = ((rgba >> 0) & 0xFF) / 255.0f;
    float g = ((rgba >> 8) & 0xFF) / 255.0f;
    float b = ((rgba >> 16) & 0xFF) / 255.0f;
    float a = ((rgba >> 24) & 0xFF) / 255.0f;

    return Color4D(r, g, b, a);
}

/**
 * Converts the given Color4D object to the range of 0-255 for each color component.
 *
 * @param color The Color4D object to be converted.
 * @return The converted Color4D object with color components in the range of 0-255.
 */
Color4D Utils::convertTo255(const Color4D &color)
{
    int r = static_cast<int>(round(color.getX() * 255));
    int g = static_cast<int>(round(color.getY() * 255));
    int b = static_cast<int>(round(color.getZ() * 255));
    int a = static_cast<int>(round(color.getW() * 255));

    return Color4D(r, g, b, a);
}

/**
 * Converts the given color to a normalized color in the range [0, 1].
 *
 * @param color The color to be converted.
 * @return The normalized color in the range [0, 1].
 */
Color4D Utils::convertTo01(const Color4D &color)
{
    float r = color.getX() / 255.0f;
    float g = color.getY() / 255.0f;
    float b = color.getZ() / 255.0f;
    float a = color.getW() / 255.0f;

    return Color4D(r, g, b, a);
}
