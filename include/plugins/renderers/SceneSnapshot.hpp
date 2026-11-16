/*
** EPITECH PROJECT, 2024
** raytracer
** File description:
** SceneSnapshot
*/

#ifndef SCENE_SNAPSHOT_HPP_
#define SCENE_SNAPSHOT_HPP_

#include "Background.hpp"
#include "scene/Atmosphere.hpp"
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
class Texture;

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
    int textureIndex = -1;
    int normalTextureIndex = -1;
    int roughnessTextureIndex = -1;
    int emissionTextureIndex = -1;
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
    std::vector<std::shared_ptr<const Texture>> textures;
    std::vector<int> meshMaterialMaps;
    Background background;
    Atmosphere atmosphere;
};

CameraData makeCameraData(const Camera &camera);
std::vector<LightData> collectLights(const Scene &scene);
SceneSnapshot buildSceneSnapshot(const Scene &scene, const Camera &camera);

/**
 * Refreshes only the animated parts of an existing snapshot: camera,
 * primitives, mesh material-group ranges and lights. Materials, textures,
 * meshes, background and atmosphere are reused. Returns false when the scene
 * contains a mesh asset that was not present in the original snapshot.
 */
bool refreshSceneSnapshot(SceneSnapshot &snapshot, const Scene &scene, const Camera &camera);

} // namespace RayTracer

#endif // SCENE_SNAPSHOT_HPP_
