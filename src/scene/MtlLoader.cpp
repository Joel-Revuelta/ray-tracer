/*
** EPITECH PROJECT, 2024
** raytracer
** File description:
** MtlLoader
*/

#include "MtlLoader.hpp"
#include <algorithm>
#include <cctype>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <string>

using namespace RayTracer;

namespace
{
[[noreturn]] void fail(const std::string &path, std::size_t line, const std::string &message)
{
    throw std::runtime_error(path + ":" + std::to_string(line) + ": " + message);
}

std::string trim(std::string value)
{
    while (!value.empty() && std::isspace(static_cast<unsigned char>(value.front())))
        value.erase(value.begin());
    while (!value.empty() && std::isspace(static_cast<unsigned char>(value.back())))
        value.pop_back();
    return value;
}

Color3D parseColor(std::istringstream &stream, const std::string &path, std::size_t line, const std::string &field)
{
    double red = 0.0;
    double green = 0.0;
    double blue = 0.0;

    if (!(stream >> red >> green >> blue) || !std::isfinite(red) || !std::isfinite(green) ||
        !std::isfinite(blue))
        fail(path, line, "invalid " + field + " value");

    return Color3D(std::clamp(static_cast<float>(red), 0.0f, 1.0f),
                   std::clamp(static_cast<float>(green), 0.0f, 1.0f),
                   std::clamp(static_cast<float>(blue), 0.0f, 1.0f));
}

float parseFloat(std::istringstream &stream, const std::string &path, std::size_t line, const std::string &field)
{
    double value = 0.0;

    if (!(stream >> value) || !std::isfinite(value))
        fail(path, line, "invalid " + field + " value");

    return static_cast<float>(value);
}
} // namespace

std::vector<MtlMaterial> RayTracer::loadMtl(const std::string &path)
{
    std::ifstream file(path);

    if (!file)
        throw std::runtime_error("Cannot open MTL file: " + path);

    const std::filesystem::path mtlPath(path);
    std::vector<MtlMaterial> materials;
    MtlMaterial *current = nullptr;
    std::string line;
    std::size_t lineNumber = 0;

    while (std::getline(file, line))
    {
        ++lineNumber;

        if (!line.empty() && line.back() == '\r')
            line.pop_back();

        const std::size_t comment = line.find('#');

        if (comment != std::string::npos)
            line.erase(comment);

        std::istringstream stream(line);
        std::string tag;

        if (!(stream >> tag))
            continue;

        if (tag == "newmtl")
        {
            std::string name;

            std::getline(stream >> std::ws, name);
            name = trim(name);

            if (name.empty())
                fail(path, lineNumber, "empty newmtl name");

            materials.emplace_back();
            current = &materials.back();
            current->name = name;
            continue;
        }

        if (current == nullptr)
            fail(path, lineNumber, "material property before newmtl");

        if (tag == "Ka")
            current->ambient = parseColor(stream, path, lineNumber, "Ka");
        else if (tag == "Kd")
            current->diffuse = parseColor(stream, path, lineNumber, "Kd");
        else if (tag == "Ks")
            current->specular = parseColor(stream, path, lineNumber, "Ks");
        else if (tag == "Ke")
            current->emission = parseColor(stream, path, lineNumber, "Ke");
        else if (tag == "Ns")
            current->shininess = std::clamp(parseFloat(stream, path, lineNumber, "Ns"), 0.0f, 1000.0f);
        else if (tag == "d")
            current->opacity = std::clamp(parseFloat(stream, path, lineNumber, "d"), 0.0f, 1.0f);
        else if (tag == "Tr")
            current->opacity = std::clamp(1.0f - parseFloat(stream, path, lineNumber, "Tr"), 0.0f, 1.0f);
        else if (tag == "map_Kd")
        {
            std::string mapPath;

            std::getline(stream >> std::ws, mapPath);
            mapPath = trim(mapPath);

            if (mapPath.empty())
                fail(path, lineNumber, "empty map_Kd path");
            if (mapPath.front() == '-')
                fail(path, lineNumber, "map_Kd options are not supported");

            current->diffuseMap = (mtlPath.parent_path() / std::filesystem::path(mapPath)).lexically_normal().string();
        }
    }

    if (materials.empty())
        throw std::runtime_error(path + ": MTL file contains no materials");

    return materials;
}
