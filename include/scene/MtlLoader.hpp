/*
** EPITECH PROJECT, 2024
** raytracer
** File description:
** MtlLoader
*/

#ifndef MTL_LOADER_HPP_
#define MTL_LOADER_HPP_

#include "Color.hpp"
#include <string>
#include <vector>

namespace RayTracer
{

/**
 * Subset of a Wavefront MTL material supported by the renderer.
 *
 * Kd is the diffuse colour, Ka/Ks are reduced to scalar reflection
 * coefficients by the parser, Ns is the Phong shininess and map_Kd is an
 * optional albedo texture resolved relative to the MTL file.
 */
struct MtlMaterial
{
    std::string name;
    Color3D ambient{1.0f, 1.0f, 1.0f};
    Color3D diffuse{0.8f, 0.8f, 0.8f};
    Color3D specular{0.0f, 0.0f, 0.0f};
    Color3D emission{0.0f, 0.0f, 0.0f};
    float shininess = 0.0f;
    float opacity = 1.0f;
    std::string diffuseMap;
};

/**
 * Loads a Wavefront MTL file. Unsupported fields are ignored. Throws
 * std::runtime_error with the file and line on malformed input.
 */
std::vector<MtlMaterial> loadMtl(const std::string &path);

} // namespace RayTracer

#endif // MTL_LOADER_HPP_
