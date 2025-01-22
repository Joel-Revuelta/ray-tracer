/*
** EPITECH PROJECT, 2023
** raytracer
** File description:
** PrimitiveFactory.cpp
*/

#include "Factory.hpp"

using namespace RayTracer;

Factory::Factory(void)
    : _map({{"Sphere", "raytracer_sphere.so"},
            {"Plane", "raytracer_plane.so"},
            {"Cylinder", "raytracer_cylinder.so"},
            {"Cone", "raytracer_cone.so"},
            {"Rotation", "raytracer_transformation_rotation.so"},
            {"Translation", "raytracer_transformation_translation.so"},
            {"AmbientLight", "raytracer_light_ambient.so"},
            {"PointLight", "raytracer_light_point.so"},
            {"DirectionalLight", "raytracer_light_directional.so"},
            {"SFML", "raytracer_gui.so"},
            {"Material", "raytracer_material_custom.so"},
            {"EmissiveMaterial", "raytracer_material_emissive.so"},
            {"FlatColorMaterial", "raytracer_material_flat.so"},
            {"CustomMaterial", "raytracer_material_custom.so"},
            {"DiffuseMaterial", "raytracer_material_diffuse.so"},
            {"MetallicMaterial", "raytracer_material_metallic.so"}})
{
}

DLManager Factory::_dlManager;
