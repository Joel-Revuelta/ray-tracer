/*
** EPITECH PROJECT, 2024
** raytracer
** File description:
** SceneSnapshot
*/

#ifndef SCENE_SNAPSHOT_HPP_
#define SCENE_SNAPSHOT_HPP_

#include "Background.hpp"
#include "IMaterial.hpp"
#include "LightData.hpp"
#include "Mesh.hpp"
#include "PrimitiveData.hpp"
#include "Vec3D.hpp"
#include <memory>
#include <vector>

namespace RayTracer
{

class Camera;
class Scene;

struct MaterialData
{
    IMaterial::MaterialType type = IMaterial::DIFFUSE;
    Color3D diffuseColor;
    Color3D emissionColor;
    float ambientReflection = 0.0f;
    float diffuseReflection = 0.0f;
    float specularReflection = 0.0f;
    float ior = 1.5f;
    float roughness = 0.0f;
};

struct CameraData
{
    Vec3D lookFrom;
    Vec3D initialPixelPos;
    Vec3D pixelWidthDistance;
    Vec3D pixelHeightDistance;
    Vec3D viewRight;
    Vec3D viewUp;
    int imageWidth = 0;
    int imageHeight = 0;
    float verticalFovDegree = 0.0f;
    float lensRadius = 0.0f;
    float focusDistance = 0.0f;
    float focalDistance = 0.0f;
};

/**
 * Flat, device-friendly representation of a scene, intended for renderers
 * that cannot traverse the plugin-backed scene graph directly.
 */
struct SceneSnapshot
{
    CameraData camera;
    std::vector<PrimitiveData> primitives;
    std::vector<MaterialData> materials;
    std::vector<LightData> lights;
    std::vector<std::shared_ptr<const MeshAsset>> meshes;
    Background background;
};

SceneSnapshot buildSceneSnapshot(const Scene &scene, const Camera &camera);

} // namespace RayTracer

#endif // SCENE_SNAPSHOT_HPP_
