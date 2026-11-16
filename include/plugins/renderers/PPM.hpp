/*
** EPITECH PROJECT, 2024
** raytracer
** File description:
** PPM
*/

#ifndef PPM_HPP_
#define PPM_HPP_

#include <cstddef>
#include <cstdint>
#include <ostream>
#include <string>
#include <vector>

namespace RayTracer
{

/**
 * Decoded PPM image with pixels in the renderer's packed RGBA layout:
 * `0xAABBGGRR`. P3 and P6 inputs are accepted, comments are allowed in the
 * header, and 8-bit output is produced for both.
 */
struct PPMImage
{
    std::size_t width = 0;
    std::size_t height = 0;
    std::vector<uint32_t> pixels;
};

void writePPM(std::ostream &out, int width, int height, const std::vector<uint32_t> &pixels);
void writePPMFile(const std::string &path, int width, int height, const std::vector<uint32_t> &pixels);

/**
 * Reads a PPM image for display or post-processing. Throws std::runtime_error
 * with the path and reason on malformed, oversized or truncated input.
 */
PPMImage readPPMFile(const std::string &path);

} // namespace RayTracer

#endif // PPM_HPP_
