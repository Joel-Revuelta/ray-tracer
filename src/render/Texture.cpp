/*
** EPITECH PROJECT, 2024
** raytracer
** File description:
** Texture
*/

#include "render/Texture.hpp"
#include <cerrno>
#include <cctype>
#include <cmath>
#include <cstdlib>
#include <fstream>
#include <limits>
#include <sstream>
#include <stdexcept>
#include <utility>

using namespace RayTracer;

namespace
{
constexpr std::size_t MAX_TEXELS = 16000000;

[[noreturn]] void fail(const std::string &path, const std::string &message)
{
    throw std::runtime_error(path + ": " + message);
}

bool readToken(std::istream &stream, std::string &token)
{
    token.clear();

    while (true)
    {
        const int next = stream.peek();

        if (next == '#')
        {
            std::string comment;

            std::getline(stream, comment);
            continue;
        }
        if (next == std::char_traits<char>::eof())
            return false;
        if (std::isspace(static_cast<unsigned char>(next)))
        {
            stream.get();
            continue;
        }
        break;
    }

    while (true)
    {
        const int next = stream.peek();

        if (next == std::char_traits<char>::eof() || std::isspace(static_cast<unsigned char>(next)) || next == '#')
            break;
        token.push_back(static_cast<char>(stream.get()));
    }

    return !token.empty();
}

int readInteger(std::istream &stream, const std::string &path, const std::string &name, int minimum, int maximum)
{
    std::string token;

    if (!readToken(stream, token))
        fail(path, "missing " + name);

    errno = 0;
    char *end = nullptr;
    const long value = std::strtol(token.c_str(), &end, 10);

    if (errno != 0 || end == token.c_str() || *end != '\0' || value < minimum || value > maximum)
        fail(path, "invalid " + name + " '" + token + "'");

    return static_cast<int>(value);
}
} // namespace

Texture::Texture(int width, int height, std::vector<Color3D> pixels)
    : _width(width), _height(height), _pixels(std::move(pixels))
{
    if (_width <= 0 || _height <= 0)
        throw std::invalid_argument("Texture dimensions must be positive");
    if (_pixels.size() != static_cast<std::size_t>(_width) * static_cast<std::size_t>(_height))
        throw std::invalid_argument("Texture pixel count does not match its dimensions");
}

int Texture::getWidth(void) const
{
    return _width;
}

int Texture::getHeight(void) const
{
    return _height;
}

const std::vector<Color3D> &Texture::getPixels(void) const
{
    return _pixels;
}

Color3D Texture::texel(int x, int y) const
{
    x %= _width;
    if (x < 0)
        x += _width;
    y %= _height;
    if (y < 0)
        y += _height;
    return _pixels[static_cast<std::size_t>(y) * static_cast<std::size_t>(_width) + static_cast<std::size_t>(x)];
}

Color3D Texture::sample(float u, float v) const
{
    u -= std::floor(u);
    v -= std::floor(v);

    const float x = u * static_cast<float>(_width) - 0.5f;
    const float y = v * static_cast<float>(_height) - 0.5f;
    const int x0 = static_cast<int>(std::floor(x));
    const int y0 = static_cast<int>(std::floor(y));
    const float fx = x - static_cast<float>(x0);
    const float fy = y - static_cast<float>(y0);

    const Color3D top = texel(x0, y0) * (1.0f - fx) + texel(x0 + 1, y0) * fx;
    const Color3D bottom = texel(x0, y0 + 1) * (1.0f - fx) + texel(x0 + 1, y0 + 1) * fx;

    return top * (1.0f - fy) + bottom * fy;
}

std::shared_ptr<const Texture> RayTracer::loadPPMTexture(const std::string &path)
{
    std::ifstream file(path, std::ios::binary);

    if (!file)
        throw std::runtime_error("Cannot open texture file: " + path);

    std::string magic;

    if (!readToken(file, magic) || (magic != "P3" && magic != "P6"))
        fail(path, "expected a P3 or P6 PPM file");

    const int width = readInteger(file, path, "width", 1, 1000000);
    const int height = readInteger(file, path, "height", 1, 1000000);
    const int maxValue = readInteger(file, path, "maximum value", 1, 65535);
    const std::size_t texelCount = static_cast<std::size_t>(width) * static_cast<std::size_t>(height);

    if (texelCount > MAX_TEXELS)
        fail(path, "texture is too large");

    std::vector<Color3D> pixels;

    pixels.reserve(texelCount);

    if (magic == "P6")
    {
        if (maxValue > 255)
            fail(path, "16-bit P6 textures are not supported");

        const int separator = file.get();

        if (separator == std::char_traits<char>::eof())
            fail(path, "missing binary texture data");
        if (separator == '\r' && file.peek() == '\n')
            file.get();

        const float inverseMaximum = 1.0f / static_cast<float>(maxValue);

        for (std::size_t i = 0; i < texelCount; ++i)
        {
            unsigned char rgb[3];

            file.read(reinterpret_cast<char *>(rgb), sizeof(rgb));

            if (file.gcount() != static_cast<std::streamsize>(sizeof(rgb)))
                fail(path, "truncated binary texture data");

            pixels.emplace_back(static_cast<float>(rgb[0]) * inverseMaximum,
                                static_cast<float>(rgb[1]) * inverseMaximum,
                                static_cast<float>(rgb[2]) * inverseMaximum);
        }
    }
    else
    {
        const float inverseMaximum = 1.0f / static_cast<float>(maxValue);

        for (std::size_t i = 0; i < texelCount; ++i)
        {
            const int red = readInteger(file, path, "red component", 0, maxValue);
            const int green = readInteger(file, path, "green component", 0, maxValue);
            const int blue = readInteger(file, path, "blue component", 0, maxValue);

            pixels.emplace_back(static_cast<float>(red) * inverseMaximum, static_cast<float>(green) * inverseMaximum,
                                static_cast<float>(blue) * inverseMaximum);
        }
    }

    return std::make_shared<Texture>(width, height, std::move(pixels));
}
