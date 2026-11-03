/*
** EPITECH PROJECT, 2024
** raytracer
** File description:
** SceneSnapshot
*/

#include "plugins/renderers/SceneSnapshot.hpp"
#include "Camera.hpp"
#include "Scene.hpp"

using namespace RayTracer;

SceneSnapshot RayTracer::buildSceneSnapshot(const Scene &scene, const Camera &camera)
{
    SceneSnapshot snapshot;

    snapshot.camera.lookFrom = camera.getLookFrom();
    snapshot.camera.initialPixelPos = camera.getInitialPixelPos();
    snapshot.camera.pixelWidthDistance = camera.getPixelWidthDistance();
    snapshot.camera.pixelHeightDistance = camera.getPixelHeightDistance();
    snapshot.camera.imageWidth = camera.getImageWidth();
    snapshot.camera.imageHeight = camera.getImageHeight();
    snapshot.camera.verticalFovDegree = camera.getVerticalFovDegree();
    snapshot.background = scene.getBackground();

    for (const auto &material : scene.getMaterials())
    {
        MaterialData data;

        data.type = material->getType();
        data.diffuseColor = material->getDiffuseColor();
        data.emissionColor = material->getEmissionColor();
        data.ambientReflection = material->getAmbientReflectionCoefficient();
        data.diffuseReflection = material->getDiffuseReflectionCoefficient();
        data.specularReflection = material->getSpecularReflectionCoefficient();
        data.ior = material->getIndexOfRefraction();
        snapshot.materials.push_back(data);
    }

    for (const auto &light : scene.getLights())
    {
        LightData data;

        if (light->getData(data))
            snapshot.lights.push_back(data);
    }

    for (const auto &primitive : scene.getPrimitives())
    {
        PrimitiveData data;

        if (primitive->getData(data))
            snapshot.primitives.push_back(data);
    }

    return snapshot;
}
