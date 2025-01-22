/*
** EPITECH PROJECT, 2023
** raytracer
** File description:
** DLManager.cpp
*/

#include "DLManager.hpp"

using namespace RayTracer;

DLManager::DLManager(void)
{
}

DLManager::DLManager(const char *libraryPath) : currentHandle(nullptr), _libPath(libraryPath)
{
    openLib(_libPath);
}

DLManager::~DLManager(void)
{
    closeLib();
}

/**
 * Opens a dynamic library specified by the given library path.
 *
 * @param libraryPath The path to the dynamic library to be opened.
 */
void DLManager::openLib(const std::string &libraryPath)
{
    std::string prePath = "./plugins/";

    auto it = handles.find(libraryPath);
    if (it != handles.end())
    {
        _libPath = it->first;
        currentHandle = it->second;
        return;
    }

    if (libraryPath.find("plugins/") != std::string::npos || libraryPath.find("./plugins/") != std::string::npos)
        prePath = "";
    currentHandle = dlopen((prePath + libraryPath).c_str(), RTLD_LAZY);
    if (currentHandle == NULL)
    {
        throw std::runtime_error(dlerror());
    }
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
    if (currentHandle)
    {
        if (dlclose(currentHandle) != 0)
            throw std::runtime_error(dlerror());

        auto it = handles.find(_libPath);
        if (it != handles.end())
            handles.erase(it);
        currentHandle = nullptr;
    }
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
