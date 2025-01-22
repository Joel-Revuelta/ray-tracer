/*
** EPITECH PROJECT, 2023
** raytracer
** File description:
** PrimitiveFactory.hpp
*/

#ifndef FACTORY_HPP_
#define FACTORY_HPP_

#include <DLManager.hpp>
#include <Primitive.hpp>
#include <memory>
#include <string>
#include <unordered_map>

namespace RayTracer
{

class Factory
{
  public:
    Factory(void);
    ~Factory(void) = default;

    template <typename T, typename... Args> std::shared_ptr<T> create(const std::string &type, Args... args)
    {
        auto it = _map.find(type);

        if (it == _map.end())
            throw std::runtime_error("type not found in library");
        _dlManager.changeLib(it->second);
        std::shared_ptr<T> obj = _dlManager.getClass<T>("create", std::forward<Args>(args)...);
        return obj;
    }

  private:
    static DLManager _dlManager;
    std::unordered_map<std::string, std::string> _map;
};

} // namespace RayTracer

#endif // FACTORY_HPP_
