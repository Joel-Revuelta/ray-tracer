/*
** EPITECH PROJECT, 2024
** raytracer
** File description:
** TemporaryPath
*/

#ifndef TEMPORARY_PATH_HPP_
#define TEMPORARY_PATH_HPP_

#include <atomic>
#include <cstdint>
#include <filesystem>
#include <string>
#include <unistd.h>

namespace RayTracer
{

/**
 * Builds a unique sibling path for an atomic write. The original extension is
 * preserved so extension-based writers keep the same format.
 */
inline std::string makeTemporaryPath(const std::string &path)
{
    static std::atomic<std::uint64_t> counter{0};

    const std::filesystem::path destination(path);
    const std::uint64_t id = counter.fetch_add(1, std::memory_order_relaxed);
    const std::string name = destination.stem().string() + ".tmp." +
                             std::to_string(static_cast<long long>(getpid())) + "." + std::to_string(id) +
                             destination.extension().string();

    return (destination.parent_path() / name).string();
}

} // namespace RayTracer

#endif // TEMPORARY_PATH_HPP_
