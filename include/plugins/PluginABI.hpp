/*
** EPITECH PROJECT, 2024
** raytracer
** File description:
** PluginABI.hpp
*/

#ifndef PLUGIN_ABI_HPP_
#define PLUGIN_ABI_HPP_

#include <cstdint>

namespace RayTracer
{

/**
 * Version of the C++ plugin interface. Bump this whenever the exported
 * factory signatures or the shared plugin data types change. Every plugin
 * exports raytracerPluginABIVersion() so the loader can reject a stale build
 * with a clear diagnostic instead of calling an incompatible function.
 */
inline constexpr std::uint32_t PLUGIN_ABI_VERSION = 1;

} // namespace RayTracer

#endif // PLUGIN_ABI_HPP_
