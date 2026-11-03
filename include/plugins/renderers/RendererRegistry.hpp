/*
** EPITECH PROJECT, 2024
** raytracer
** File description:
** RendererRegistry
*/

#ifndef RENDERER_REGISTRY_HPP_
#define RENDERER_REGISTRY_HPP_

#include <string>
#include <vector>

namespace RayTracer
{

struct RendererEntry
{
    std::string name;
    std::string factoryKey;
    std::string description;
};

const std::vector<RendererEntry> &rendererEntries(void);
const RendererEntry *findRenderer(const std::string &name);

} // namespace RayTracer

#endif // RENDERER_REGISTRY_HPP_
