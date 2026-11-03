/*
** EPITECH PROJECT, 2024
** raytracer
** File description:
** PPM
*/

#ifndef PPM_HPP_
#define PPM_HPP_

#include <cstdint>
#include <ostream>
#include <string>
#include <vector>

namespace RayTracer
{

void writePPM(std::ostream &out, int width, int height, const std::vector<uint32_t> &pixels);
void writePPMFile(const std::string &path, int width, int height, const std::vector<uint32_t> &pixels);

} // namespace RayTracer

#endif // PPM_HPP_
