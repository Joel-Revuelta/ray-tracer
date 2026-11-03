/*
** EPITECH PROJECT, 2024
** raytracer
** File description:
** SceneSnapshot
*/

#include "plugins/renderers/SceneSnapshot.hpp"
#include "Camera.hpp"
#include "Scene.hpp"
#include <cstdint>
#include <unordered_map>

using namespace RayTracer;

SceneSnapshot RayTracer::buildSceneSnapshot(const Scene &scene, const Camera &camera)
{
    SceneSnapshot snapshot;

    snapshot.camera.lookFrom = camera.getLookFrom();
    snapshot.camera.initialPixelPos = camera.getInitialPixelPos();
    snapshot.camera.pixelWidthDistance = camera.getPixelWidthDistance();
    snapshot.camera.pixelHeightDistance = camera.getPixelHeightDistance();
    snapshot.camera.viewRight = camera.getCamViewRight();
    snapshot.camera.viewUp = camera.getCamViewUp();
    snapshot.camera.imageWidth = camera.getImageWidth();
    snapshot.camera.imageHeight = camera.getImageHeight();
    snapshot.camera.verticalFovDegree = camera.getVerticalFovDegree();
    snapshot.camera.lensRadius = camera.getLensRadius();
    snapshot.camera.focusDistance = camera.getFocusDistance();
    snapshot.camera.focalDistance = static_cast<float>(camera.getFocalDistance());
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
        data.roughness = material->getRoughness();
        snapshot.materials.push_back(data);
    }

    for (const auto &light : scene.getLights())
    {
        LightData data;

        if (light->getData(data))
            snapshot.lights.push_back(data);
    }

    std::unordered_map<const MeshAsset *, std::uint32_t> meshIndices;

    for (const auto &primitive : scene.getPrimitives())
    {
        PrimitiveData data;

        if (!primitive->getData(data))
            continue;

        if (data.type == PrimitiveType::MESH)
        {
            const std::shared_ptr<const MeshAsset> asset = primitive->getMeshAsset();

            if (asset)
            {
                const auto inserted =
                    meshIndices.emplace(asset.get(), static_cast<std::uint32_t>(snapshot.meshes.size()));

                if (inserted.second)
                    snapshot.meshes.push_back(asset);

                data.meshIndex = inserted.first->second;
            }
        }

        snapshot.primitives.push_back(data);
    }

    return snapshot;
}
