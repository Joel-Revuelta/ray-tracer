/*
** EPITECH PROJECT, 2023
** raytracer
** File description:
** RayTracerException.hpp
*/

#ifndef RAYTRACER_EXCEPTION_HPP_
#define RAYTRACER_EXCEPTION_HPP_

#include <string>

namespace RayTracer
{

class RayTracerException : public std::exception
{
  public:
    RayTracerException(void){};
    RayTracerException(const std::string &msg) : _message(msg){};
    virtual ~RayTracerException(void) = default;

    const char *what(void) const noexcept override
    {
        return _message.c_str();
    }

  protected:
    std::string _message = "RayTracerException: something went wrong.\n";
};

} // namespace RayTracer

#endif
