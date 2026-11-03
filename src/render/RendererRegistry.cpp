/*
** EPITECH PROJECT, 2024
** raytracer
** File description:
** RendererRegistry
*/

#include "plugins/renderers/RendererRegistry.hpp"
#include <algorithm>
#include <cctype>

using namespace RayTracer;

namespace
{
const std::vector<RendererEntry> ENTRIES = {
    {"cpu", "RendererCPU", "Multithreaded CPU renderer (default)"},
    {"cuda", "RendererCUDA", "CUDA GPU renderer"},
};

std::string toLower(const std::string &value)
{
    std::string result = value;

    std::transform(result.begin(), result.end(), result.begin(),
                   [](unsigned char character) { return static_cast<char>(std::tolower(character)); });
    return result;
}
} // namespace

const std::vector<RendererEntry> &RayTracer::rendererEntries(void)
{
    return ENTRIES;
}

const RendererEntry *RayTracer::findRenderer(const std::string &name)
{
    const std::string lowerName = toLower(name);

    for (const RendererEntry &entry : ENTRIES)
    {
        if (entry.name == lowerName)
            return &entry;
    }
    return nullptr;
}
