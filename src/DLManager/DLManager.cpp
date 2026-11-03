/*
** EPITECH PROJECT, 2023
** raytracer
** File description:
** DLManager.cpp
*/

#include "DLManager.hpp"
#include <cstdlib>
#include <system_error>
#include <vector>

using namespace RayTracer;

namespace
{
std::filesystem::path executableDirectory(void)
{
    std::error_code error;
    const std::filesystem::path executable = std::filesystem::read_symlink("/proc/self/exe", error);

    if (error)
        return {};
    return executable.parent_path();
}

std::string resolvePluginPath(const std::string &libraryPath)
{
    const std::filesystem::path libraryFile(libraryPath);

    if (libraryFile.is_absolute() || libraryFile.has_parent_path())
        return libraryPath;

    std::vector<std::filesystem::path> candidates;

    if (const char *pluginDirectory = std::getenv("RAYTRACER_PLUGIN_DIR"); pluginDirectory != nullptr &&
                                                                           *pluginDirectory != '\0')
        candidates.emplace_back(std::filesystem::path(pluginDirectory) / libraryPath);

    candidates.emplace_back(std::filesystem::path("./plugins") / libraryPath);

    const std::filesystem::path executableDir = executableDirectory();

    if (!executableDir.empty())
    {
        candidates.emplace_back(executableDir / "plugins" / libraryPath);
        candidates.emplace_back(executableDir / ".." / "lib" / "raytracer" / "plugins" / libraryPath);
    }

    for (const std::filesystem::path &candidate : candidates)
    {
        std::error_code error;

        if (std::filesystem::exists(candidate, error) && !error)
            return candidate.string();
    }

    return candidates.front().string();
}
} // namespace

DLManager::DLManager(void) : currentHandle(nullptr)
{
}

DLManager::DLManager(const char *libraryPath) : currentHandle(nullptr), _libPath(libraryPath)
{
    openLib(_libPath);
}

DLManager::~DLManager(void) noexcept
{
    try
    {
        closeLib();
    }
    catch (...)
    {
    }
}

/**
 * Opens a dynamic library specified by the given library path.
 *
 * @param libraryPath The path to the dynamic library to be opened.
 */
void DLManager::openLib(const std::string &libraryPath)
{
    auto it = handles.find(libraryPath);
    if (it != handles.end())
    {
        _libPath = it->first;
        currentHandle = it->second;
        return;
    }

    const std::string resolvedPath = resolvePluginPath(libraryPath);
    void *handle = dlopen(resolvedPath.c_str(), RTLD_NOW | RTLD_LOCAL);

    if (handle == nullptr)
    {
        const char *error = dlerror();
        throw std::runtime_error(error != nullptr ? error : "Failed to open dynamic library");
    }

    currentHandle = handle;
    _libPath = libraryPath;
    handles.insert(std::make_pair(_libPath, currentHandle));
}

/**
 * @brief Closes the dynamic library.
 *
 * This function is responsible for closing the dynamic library that was previously loaded.
 * It should be called when the library is no longer needed to free up system resources.
 */
void DLManager::closeLib(void)
{
    for (const auto &entry : handles)
    {
        if (entry.second != nullptr)
            dlclose(entry.second);
    }

    handles.clear();
    currentHandle = nullptr;
    _libPath.clear();
}

/**
 * Retrieves the name of the library associated with the DLManager object.
 *
 * @return A string representing the name of the library.
 */
std::string DLManager::getLib(void) const
{
    std::filesystem::path filePath(_libPath);
    return filePath.filename().string();
}

/**
 * @brief Changes the library used by the DLManager.
 *
 * This function allows you to change the library used by the DLManager to load dynamic libraries.
 *
 * @param libraryPath The path to the new library.
 */
void DLManager::changeLib(const std::string &libraryPath)
{
    openLib(libraryPath);
}
