/*
** EPITECH PROJECT, 2024
** raytracer
** File description:
** Texture
*/

#ifndef TEXTURE_HPP_
#define TEXTURE_HPP_

#include "Color.hpp"
#include <memory>
#include <string>
#include <vector>

namespace RayTracer
{

/**
 * Bilinear, repeat-wrapped RGB texture. Texels are stored as linear floats in
 * [0, 1]; PPM input is converted from its 8-bit representation on load.
 */
class Texture
{
  public:
    Texture(int width, int height, std::vector<Color3D> pixels);

    int getWidth(void) const;
    int getHeight(void) const;
    const std::vector<Color3D> &getPixels(void) const;

    Color3D sample(float u, float v) const;

  private:
    Color3D texel(int x, int y) const;

    int _width;
    int _height;
    std::vector<Color3D> _pixels;
};

/**
 * Loads a binary P6 or ASCII P3 PPM file. Throws std::runtime_error with the
 * path and reason on malformed input.
 */
std::shared_ptr<const Texture> loadPPMTexture(const std::string &path);

} // namespace RayTracer

#endif // TEXTURE_HPP_
