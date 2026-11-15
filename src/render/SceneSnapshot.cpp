/*
** EPITECH PROJECT, 2024
** raytracer
** File description:
** SceneSnapshot
*/

#include "plugins/renderers/SceneSnapshot.hpp"
#include "Camera.hpp"
#include "Scene.hpp"
#include "render/Texture.hpp"
#include <cstdint>
#include <stdexcept>
#include <unordered_map>

using namespace RayTracer;

CameraData RayTracer::makeCameraData(const Camera &camera)
{
    CameraData data;

    data.lookFrom = camera.getLookFrom();
    data.initialPixelPos = camera.getInitialPixelPos();
    data.pixelWidthDistance = camera.getPixelWidthDistance();
    data.pixelHeightDistance = camera.getPixelHeightDistance();
    data.viewRight = camera.getCamViewRight();
    data.viewUp = camera.getCamViewUp();
    data.imageWidth = camera.getImageWidth();
    data.imageHeight = camera.getImageHeight();
    data.verticalFovDegree = camera.getVerticalFovDegree();
    data.lensRadius = camera.getLensRadius();
    data.focusDistance = camera.getFocusDistance();
    data.focalDistance = static_cast<float>(camera.getFocalDistance());
    return data;
}

SceneSnapshot RayTracer::buildSceneSnapshot(const Scene &scene, const Camera &camera)
{
    SceneSnapshot snapshot;

    snapshot.camera = makeCameraData(camera);
    snapshot.background = scene.getBackground();

    std::unordered_map<const Texture *, int> textureIndices;

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

        const std::shared_ptr<const Texture> texture = material->getTexture();

        if (texture)
        {
            const auto inserted =
                textureIndices.emplace(texture.get(), static_cast<int>(snapshot.textures.size()));

            if (inserted.second)
                snapshot.textures.push_back(texture);

            data.textureIndex = inserted.first->second;
        }

        snapshot.materials.push_back(data);
    }

    if (snapshot.background.environment)
    {
        const auto inserted = textureIndices.emplace(snapshot.background.environment.get(),
                                                     static_cast<int>(snapshot.textures.size()));

        if (inserted.second)
            snapshot.textures.push_back(snapshot.background.environment);

        snapshot.background.environmentTextureIndex = inserted.first->second;
    }

    for (const auto &light : scene.getLights())
    {
        LightData data;

        if (!light->getData(data))
            throw std::runtime_error("A light does not support renderer-neutral snapshots");

        snapshot.lights.push_back(data);
    }

    std::unordered_map<const MeshAsset *, std::uint32_t> meshIndices;

    for (const auto &primitive : scene.getPrimitives())
    {
        PrimitiveData data;

        if (!primitive->getData(data))
            throw std::runtime_error("A primitive does not support renderer-neutral snapshots");

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

            const std::shared_ptr<const std::vector<int>> materialMap = primitive->getMeshMaterialMap();

            if (materialMap && !materialMap->empty())
            {
                data.meshMaterialMapStart = static_cast<int>(snapshot.meshMaterialMaps.size());
                data.meshMaterialMapCount = static_cast<int>(materialMap->size());
                snapshot.meshMaterialMaps.insert(snapshot.meshMaterialMaps.end(), materialMap->begin(),
                                                 materialMap->end());
            }
        }

        snapshot.primitives.push_back(data);
    }

    return snapshot;
}
