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
 * Bilinear, repeat-wrapped RGB texture. Texels are stored as linear,
 * non-negative floats. LDR input is sRGB-decoded on load; Radiance HDR input
 * keeps its full high-dynamic-range values.
 */
class Texture
{
  public:
    Texture(int width, int height, std::vector<Color3D> pixels);

    int getWidth(void) const;
    int getHeight(void) const;
    const std::vector<Color3D> &getPixels(void) const;

    Color3D sample(float u, float v) const;
    Color3D sampleClamped(float u, float v) const;

  private:
    Color3D texel(int x, int y, bool clampY) const;
    Color3D sampleInternal(float u, float v, bool clampV) const;

    int _width;
    int _height;
    std::vector<Color3D> _pixels;
};

enum class TextureColorSpace
{
    SRGB,
    LINEAR
};

/**
 * Loads a binary P6 or ASCII P3 PPM file. Throws std::runtime_error with the
 * path and reason on malformed input.
 */
std::shared_ptr<const Texture> loadPPMTexture(const std::string &path,
                                              TextureColorSpace colorSpace = TextureColorSpace::SRGB);

/**
 * Loads an image by extension. PPM uses the in-tree parser, Radiance HDR
 * keeps linear HDR values, and PNG/JPG are decoded according to colorSpace.
 */
std::shared_ptr<const Texture> loadTextureFile(const std::string &path,
                                               TextureColorSpace colorSpace = TextureColorSpace::SRGB);

/**
 * Loads a Radiance RGBE HDR image using stb_image.
 */
std::shared_ptr<const Texture> loadHDRTexture(const std::string &path);

} // namespace RayTracer

#endif // TEXTURE_HPP_
