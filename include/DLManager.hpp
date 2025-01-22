/*
** EPITECH PROJECT, 2023
** raytracer
** File description:
** DLManager.hpp
*/

#ifndef DLMANAGER_HPP_
#define DLMANAGER_HPP_

#include <dlfcn.h>
#include <filesystem>
#include <iostream>
#include <unistd.h>
#include <unordered_map>

namespace RayTracer
{
class DLManager
{
  public:
    DLManager(void);
    DLManager(const char *libraryPath);
    ~DLManager(void);

    void closeLib(void);
    void openLib(const std::string &libraryPath);
    void changeLib(const std::string &libraryPath);

    std::string getLib(void) const;

    /**
     * Retrieves a pointer to a class instance based on the provided class initialization ID.
     *
     * @tparam FuncType The type of the class instance to retrieve.
     * @param classInitID The initialization ID of the class instance.
     * @return A pointer to the class instance if found, nullptr otherwise.
     */
    template <typename T, typename... Args> std::shared_ptr<T> getClass(const char *classInitID, Args... args)
    {
        typedef std::shared_ptr<T> (*tmp)(Args...);
        tmp classInit;

        if (!currentHandle)
            throw std::runtime_error("Tried to get class without opening a library.");
        classInit = reinterpret_cast<tmp>(dlsym(currentHandle, classInitID));
        if (!classInit)
            throw std::runtime_error((dlerror()));
        return classInit(std::forward<Args>(args)...);
    }

  private:
    void *currentHandle;
    std::string _libPath;
    std::unordered_map<std::string, void *> handles;
};
} // namespace RayTracer

#endif // DLMANAGER_HPP_
