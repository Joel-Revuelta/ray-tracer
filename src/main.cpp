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
#include "Renderer.hpp"
#include "SceneBuilder.hpp"
#include "Vec3D.hpp"

using namespace RayTracer;

int main(int ac, char **av)
{
    /* Factory factory;
    SceneBuilder sceneBuilder;

    Color3D blue(0.0, 0.0, 1.0);
    Color3D white(1.0, 1.0, 1.0);
    Color3D orange(0.8, 0.5, 0.2);

    sceneBuilder.addMaterial(factory.create<IMaterial>("EmissiveMaterial", orange, 2.0, orange));
    sceneBuilder.addMaterial(factory.create<IMaterial>("DiffuseMaterial", 0.8, 0.5, white));
    sceneBuilder.addMaterial(factory.create<IMaterial>("MetallicMaterial", blue));
    sceneBuilder.addMaterial(factory.create<IMaterial>("DiffuseMaterial", 0.8, 0.5, orange));

    sceneBuilder.addLight(factory.create<ILight>("AmbientLight", Color3D(1, 1, 1), 0.1));
    // sceneBuilder.addLight(factory.create<ILight>("DirectionalLight", Vec3D(10, 0, -5), Color3D(1, 1, 1), 0.8));
    // sceneBuilder.addLight(factory.create<ILight>("PointLight", Vec3D(-50, 100, -5), white, 0.3, 0.2, 0.1, 0.01));

    sceneBuilder.addPrimitive(factory.create<IPrimitive>("Sphere", 1.0, Point3D(+2.0, 0.0, 0.0), 2));
    sceneBuilder.addPrimitive(factory.create<IPrimitive>("Sphere", 1.0, Point3D(+0.0, 2.0, 0.0), 0));
    sceneBuilder.addPrimitive(factory.create<IPrimitive>("Sphere", 1.0, Point3D(-2.0, 0.0, 0.0), 1));
    sceneBuilder.addPrimitive(factory.create<IPrimitive>("Plane", Vec3D(0, 1, 0), Point3D(0, -1, 0), 2));

    Point3D lookFrom = Point3D(0, 2, -5);
    Point3D lookAt = Point3D(0, 0, 0);
    float fovDegree = 90.0f; // At 180.0f it changes upside down.

    auto sfml = factory.create<IGUI>("SFML", 960, 540);
    Renderer renderer(20, sceneBuilder.build(), std::make_shared<Camera>(lookAt, lookFrom, fovDegree), sfml);
    renderer.render();
    sfml->loop(); */
    RayTracer::App app(ac, av);
    app.run();
    return 0;
}
