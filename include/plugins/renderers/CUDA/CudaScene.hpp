/*
** EPITECH PROJECT, 2024
** raytracer
** File description:
** CudaScene
*/

#ifndef CUDA_SCENE_HPP_
#define CUDA_SCENE_HPP_

namespace RayTracer
{
namespace Cuda
{

// Values deliberately match PrimitiveType in PrimitiveData.hpp.
enum PrimitiveKind
{
    PRIMITIVE_NONE = 0,
    PRIMITIVE_SPHERE = 1,
    PRIMITIVE_PLANE = 2,
    PRIMITIVE_CYLINDER = 3,
    PRIMITIVE_CONE = 4
};

// Values deliberately match IMaterial::MaterialType.
enum MaterialKind
{
    MATERIAL_DIFFUSE = 0,
    MATERIAL_FLAT_COLOR = 1,
    MATERIAL_EMISSIVE = 2,
    MATERIAL_METALLIC = 3,
    MATERIAL_DIELECTRIC = 4
};

// Values deliberately match LightType in LightData.hpp.
enum LightKind
{
    LIGHT_NONE = 0,
    LIGHT_AMBIENT = 1,
    LIGHT_DIRECTIONAL = 2,
    LIGHT_POINT = 3
};

struct Vec3
{
    float x;
    float y;
    float z;
};

struct Primitive
{
    int type;
    unsigned int materialIndex;
    Vec3 center;
    Vec3 point;
    Vec3 normal;
    Vec3 axis;
    float radius;
    float height;
    int coneBaseAbove;
};

struct Material
{
    int type;
    Vec3 diffuseColor;
    Vec3 emissionColor;
    float ambientReflection;
    float diffuseReflection;
    float specularReflection;
    float ior;
};

struct Light
{
    int type;
    Vec3 color;
    Vec3 position;
    Vec3 direction;
    float intensity;
    float attenuationConstant;
    float attenuationLinear;
    float attenuationQuadratic;
};

struct Camera
{
    Vec3 lookFrom;
    Vec3 initialPixelPos;
    Vec3 pixelWidthDistance;
    Vec3 pixelHeightDistance;
    int width;
    int height;
};

struct PostProcess
{
    float exposure;
    int toneMapping;
    int srgb;
};

struct Background
{
    Vec3 zenith;
    Vec3 horizon;
    Vec3 ground;
    float intensity;
    int enabled;
};

struct BVHNode
{
    Vec3 min;
    Vec3 max;
    int left;
    int right;
    int start;
    int count;
    int leaf;
};

struct SceneView
{
    const Primitive *primitives;
    int primitiveCount;
    const Material *materials;
    int materialCount;
    const Light *lights;
    int lightCount;
    const BVHNode *nodes;
    int nodeCount;
    const int *primitiveIndices;
    int primitiveIndexCount;
    const int *unboundedIndices;
    int unboundedCount;
    Camera camera;
    Background background;
    PostProcess postProcess;
};

} // namespace Cuda
} // namespace RayTracer

#endif // CUDA_SCENE_HPP_
