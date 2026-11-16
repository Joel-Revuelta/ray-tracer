/*
** EPITECH PROJECT, 2024
** raytracer
** File description:
** ImageWriter
*/

#ifndef IMAGE_WRITER_HPP_
#define IMAGE_WRITER_HPP_

#include <cstdint>
#include <string>
#include <vector>

namespace RayTracer
{

void writePNGFile(const std::string &path, int width, int height, const std::vector<uint32_t> &pixels);
void writeImageFile(const std::string &path, int width, int height, const std::vector<uint32_t> &pixels);
void writeImageFileAtomic(const std::string &path, int width, int height, const std::vector<uint32_t> &pixels);
bool frameFileExists(const std::string &path);

/**
 * Expands a frame path pattern. Supports one printf-style integer conversion
 * such as `%04d`; paths without a conversion get a zero-padded frame suffix.
 */
std::string formatFramePath(const std::string &pattern, int frame);

} // namespace RayTracer

#endif // IMAGE_WRITER_HPP_
