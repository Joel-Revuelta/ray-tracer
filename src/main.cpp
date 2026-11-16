/*
** EPITECH PROJECT, 2024
** raytracer
** File description:
** main
*/

#include "App.hpp"
#include "Color.hpp"
#include "Factory.hpp"
#include "GUI.hpp"
#include "IMaterial.hpp"
#include "Point3D.hpp"
#include "SceneBuilder.hpp"
#include "Vec3D.hpp"
#include <cstdlib>
#include <exception>
#include <iostream>

using namespace RayTracer;

int main(int ac, char **av)
{
    try
    {
        RayTracer::App app(ac, av);
        app.run();
        return EXIT_SUCCESS;
    }
    catch (const RayTracer::AppExit &exit)
    {
        return exit.code;
    }
    catch (const std::exception &error)
    {
        std::cerr << "Error: " << error.what() << std::endl;
        return EXIT_FAILURE;
    }
}
