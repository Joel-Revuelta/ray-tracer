/*
** EPITECH PROJECT, 2024
** raytracer
** File description:
** PluginABI.cpp
*/

#include "plugins/PluginABI.hpp"

extern "C" unsigned int raytracerPluginABIVersion(void)
{
    return RayTracer::PLUGIN_ABI_VERSION;
}
