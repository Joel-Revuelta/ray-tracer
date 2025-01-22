/*
** EPITECH PROJECT, 2023
** raytracer
** File description:
** Utils.hpp
*/

#ifndef UTILS_HPP_
#define UTILS_HPP_

#include <cstdint>
#include <cstdlib>
#include <ctime>
#include <limits>
#include <random>
#include <stdexcept>

namespace RayTracer
{

class Color4D;

namespace Utils
{

constexpr const double PI = 3.1415926535897932385;

uint32_t PCGHash(uint32_t input);
uint32_t convertToRGBA(const Color4D &color);
Color4D convertTo255(const Color4D &color);
Color4D convertFromRGBA(const uint32_t &rgba);
Color4D convertTo01(const Color4D &color);

/**
 * Generates a random value within the specified range.
 *
 * @param seed The seed value used for random number generation.
 * @param min The minimum value of the range (inclusive).
 * @param max The maximum value of the range (inclusive).
 * @return A random value within the specified range.
 *
 * Note: Casting to float instead of double makes it faster but less evenly
 * distributed
 *
 */
template <typename numeric> numeric numericGeneratePseudoRandom(uint32_t &seed, numeric min, numeric max)
{
    float normalized = 0;

    if (max <= min)
        throw std::invalid_argument("max must be greater than min");

    // Generate a random value using the PCG algorithm
    seed = Utils::PCGHash(seed);

    // Normalize the random value to the range [min, max]
    normalized = (float)seed / (float)std::numeric_limits<uint32_t>::max();

    // Calculate the random value within the specified range
    return min + (numeric)(normalized * (max - min));
}

/**
 * Generates a random numeric value within the specified range.
 *
 * @tparam numeric The type of the numeric value.
 * @param min The minimum value of the range (inclusive).
 * @param max The maximum value of the range (inclusive).
 * @return A random numeric value within the specified range.
 */
template <typename numeric> numeric numericGenerateRandom(numeric min, numeric max)
{
    if (max <= min)
        throw std::invalid_argument("max must be greater than min");
    static std::uniform_real_distribution<numeric> distribution(min, max);
    static std::mt19937 generator;
    return distribution(generator);
}

/**
 * Converts degrees to radians.
 *
 * @tparam numeric The numeric type of the input value.
 * @param degrees The value in degrees to be converted.
 * @return The converted value in radians.
 */
template <typename numeric> numeric numericDegreesToRadians(numeric degrees)
{
    return degrees * (numeric)PI / (numeric)180;
}

} // namespace Utils

} // namespace RayTracer

#endif // UTILS_HPP_
