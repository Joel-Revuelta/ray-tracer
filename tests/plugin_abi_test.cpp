/*
** EPITECH PROJECT, 2024
** raytracer
** File description:
** Plugin ABI loader regression
*/

#include "DLManager.hpp"
#include <iostream>
#include <stdexcept>
#include <string>

int main(int ac, char **av)
{
    if (ac != 2)
    {
        std::cerr << "usage: " << av[0] << " <versionless-plugin>" << std::endl;
        return 2;
    }

    try
    {
        RayTracer::DLManager manager(av[1]);
        (void)manager.getClass<int>("create");
    }
    catch (const std::runtime_error &error)
    {
        const std::string message = error.what();

        if (message.find("ABI") == std::string::npos)
        {
            std::cerr << "plugin loader rejected the library for the wrong reason: " << message << std::endl;
            return 1;
        }

        return 0;
    }

    std::cerr << "plugin loader accepted a library without an ABI version" << std::endl;
    return 1;
}
