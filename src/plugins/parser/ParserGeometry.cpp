/*
** EPITECH PROJECT, 2023
** raytracer
** File description:
** ParserGeometry.cpp
*/

#include "Parser.hpp"
#include "ParserInternal.hpp"
#include "InstancePrimitive.hpp"
#include "Transform.hpp"
#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>
#include <string>

using namespace RayTracer;
using namespace RayTracer::ParserInternal;

/**
 * Parses the primitives configuration from the configuration file.
 * ! Waiting for the cylinder class to be implemented
 *
 * @param sceneBuilder The scene builder to add the primitives.
 * @param primitiveSetting The primitive setting to parse.
 */
void Parser::parsePrimitives(SceneBuilder &sceneBuilder, libconfig::Setting &primitiveSetting)
{
    rejectUnknownKeys(primitiveSetting, "Primitives",
                      {"spheres", "planes", "cylinders", "cones", "boxes", "disks", "meshes", "scatter", "instances",
                       "scene"});

    for (int i = 0; primitiveSetting.exists("spheres")
        &&  i < primitiveSetting.lookup("spheres").getLength(); i++) {
        sceneBuilder.addPrimitive(parseSphere(primitiveSetting.lookup("spheres")[i]));
    }

    for (int i = 0; primitiveSetting.exists("planes")
        && i < primitiveSetting.lookup("planes").getLength(); i++) {
        sceneBuilder.addPrimitive(parsePlane(primitiveSetting.lookup("planes")[i]));
    }

    for (int i = 0; primitiveSetting.exists("cylinders")
        && i < primitiveSetting.lookup("cylinders").getLength(); i++) {
        std::shared_ptr<IPrimitive> cylinder = parseCylinder(primitiveSetting.lookup("cylinders")[i]);

        sceneBuilder.addPrimitive(cylinder);
    }

    for (int i = 0; primitiveSetting.exists("cones")
        && i < primitiveSetting.lookup("cones").getLength(); i++) {
        std::shared_ptr<IPrimitive> cone = parseCone(primitiveSetting.lookup("cones")[i]);

        sceneBuilder.addPrimitive(cone);
    }

    for (int i = 0; primitiveSetting.exists("boxes")
        && i < primitiveSetting.lookup("boxes").getLength(); i++)
        sceneBuilder.addPrimitive(parseBox(primitiveSetting.lookup("boxes")[i]));

    for (int i = 0; primitiveSetting.exists("disks")
        && i < primitiveSetting.lookup("disks").getLength(); i++)
        sceneBuilder.addPrimitive(parseDisk(primitiveSetting.lookup("disks")[i]));

    for (int i = 0; primitiveSetting.exists("meshes")
        && i < primitiveSetting.lookup("meshes").getLength(); i++) {
        sceneBuilder.addPrimitive(parseMesh(sceneBuilder, primitiveSetting.lookup("meshes")[i]));
    }

    if (primitiveSetting.exists("scatter"))
    {
        if (_parsingPrefab)
            throw std::runtime_error("Prefabs cannot contain scatter blocks");

        const libconfig::Setting &scatter = primitiveSetting.lookup("scatter");
        const int count = scatter.isList() ? scatter.getLength() : 1;

        for (int i = 0; i < count; ++i)
        {
            const libconfig::Setting &group = scatter.isList() ? scatter[i] : scatter;

            rejectUnknownKeys(group, "Scatter", {"prefab", "count", "seed", "area", "rotation", "scale"});
            const std::string prefabName = group.lookup("prefab");
            const auto prefab = _prefabScenes.find(prefabName);

            if (prefab == _prefabScenes.end())
                throw std::runtime_error("Unknown scatter prefab: " + prefabName);

            const int instances =
                group.exists("count") ? settingAsInt(group.lookup("count"), "Scatter count", 1, 4096) : 1;
            const uint32_t seed =
                group.exists("seed") ? settingAsUInt32(group.lookup("seed"), "Scatter seed") : 0u;

            const libconfig::Setting &area = group.lookup("area");

            rejectUnknownKeys(area, "Scatter area", {"min", "max"});
            const Vec3D areaMin = parseVector(area.lookup("min"));
            const Vec3D areaMax = parseVector(area.lookup("max"));

            if (areaMin.getX() > areaMax.getX() || areaMin.getY() > areaMax.getY() ||
                areaMin.getZ() > areaMax.getZ())
                throw std::runtime_error("Scatter area minimum must not exceed its maximum");

            Vec3D rotationMin(0.0f);
            Vec3D rotationMax(0.0f);

            if (group.exists("rotation"))
            {
                const libconfig::Setting &rotation = group.lookup("rotation");

                rejectUnknownKeys(rotation, "Scatter rotation", {"min", "max"});
                rotationMin = parseVector(rotation.lookup("min"));
                rotationMax = parseVector(rotation.lookup("max"));
                if (rotationMin.getX() > rotationMax.getX() || rotationMin.getY() > rotationMax.getY() ||
                    rotationMin.getZ() > rotationMax.getZ())
                    throw std::runtime_error("Scatter rotation minimum must not exceed its maximum");
            }

            float scaleMin = 1.0f;
            float scaleMax = 1.0f;

            if (group.exists("scale"))
            {
                const libconfig::Setting &scale = group.lookup("scale");

                rejectUnknownKeys(scale, "Scatter scale", {"min", "max"});
                scaleMin = static_cast<float>(settingAsDouble(scale.lookup("min"), "Scatter scale minimum"));
                scaleMax = static_cast<float>(settingAsDouble(scale.lookup("max"), "Scatter scale maximum"));
                requirePositive(scaleMin, "Scatter scale minimum");
                requirePositive(scaleMax, "Scatter scale maximum");
                if (scaleMin > scaleMax)
                    throw std::runtime_error("Scatter scale minimum must not exceed its maximum");
            }

            std::mt19937 random(seed);
            std::uniform_real_distribution<float> distribution(0.0f, 1.0f);

            auto sample = [&](float minimum, float maximum) {
                return minimum + (maximum - minimum) * distribution(random);
            };

            for (int instance = 0; instance < instances; ++instance)
            {
                TransformPose pose;

                // Sequence the samples explicitly: function argument evaluation
                // order is unspecified and would make scatter layout
                // compiler-dependent.
                const float translationX = sample(areaMin.getX(), areaMax.getX());
                const float translationY = sample(areaMin.getY(), areaMax.getY());
                const float translationZ = sample(areaMin.getZ(), areaMax.getZ());
                const float rotationX = sample(rotationMin.getX(), rotationMax.getX());
                const float rotationY = sample(rotationMin.getY(), rotationMax.getY());
                const float rotationZ = sample(rotationMin.getZ(), rotationMax.getZ());
                const float scale = sample(scaleMin, scaleMax);

                pose.translation = Vec3D(translationX, translationY, translationZ);
                pose.rotation = Vec3D(rotationX, rotationY, rotationZ);
                pose.scale = Vec3D(scale);
                sceneBuilder.addScene(prefab->second, pose);
            }

            _materialCount = static_cast<uint>(sceneBuilder.getMaterialCount());
        }
    }

    if (primitiveSetting.exists("instances"))
    {
        if (_parsingPrefab)
            throw std::runtime_error("Prefabs cannot contain prefab instances");

        const libconfig::Setting &instances = primitiveSetting.lookup("instances");
        const int count = instances.isList() ? instances.getLength() : 1;

        for (int i = 0; i < count; ++i)
        {
            const libconfig::Setting &instance = instances.isList() ? instances[i] : instances;

            rejectUnknownKeys(instance, "Prefab instance", {"prefab", "id", "transform"});
            const std::string prefabName = instance.lookup("prefab");
            const auto prefab = _prefabScenes.find(prefabName);

            if (prefab == _prefabScenes.end())
                throw std::runtime_error("Unknown prefab: " + prefabName);

            TransformPose pose;
            if (instance.exists("transform"))
                pose = parseTransformPose(instance.lookup("transform"));

            std::string instanceId;
            if (instance.exists("id"))
            {
                instanceId = std::string(instance.lookup("id").c_str());
                if (instanceId.empty())
                    throw std::runtime_error("Instance id must not be empty");
                if (_parsingPrefab)
                    throw std::runtime_error("Prefabs cannot contain named instances");
            }

            sceneBuilder.addScene(prefab->second, pose, instanceId);
            _materialCount = static_cast<uint>(sceneBuilder.getMaterialCount());
        }
    }

    if (primitiveSetting.exists("scene"))
    {
        const libconfig::Setting &nestedScene = primitiveSetting.lookup("scene");

        rejectUnknownKeys(nestedScene, "Nested scene",
                          {"id", "transform", "materials", "primitives", "lights", "scene"});
        TransformPose pose;

        if (nestedScene.exists("transform"))
            pose = parseTransformPose(nestedScene.lookup("transform"));

        std::string instanceId;
        if (nestedScene.exists("id"))
        {
            instanceId = std::string(nestedScene.lookup("id").c_str());
            if (instanceId.empty())
                throw std::runtime_error("Nested scene id must not be empty");
            if (_parsingPrefab)
                throw std::runtime_error("Prefabs cannot contain named nested scenes");
        }

        sceneBuilder.addScene(parseScene(nestedScene), pose, instanceId);
        _materialCount = static_cast<uint>(sceneBuilder.getMaterialCount());
    }
}

/**
 * Parses the scene configuration from the configuration file.
 * Builds the scene object adding the camera, lights, primitives and another scene(if exists).
 *
 * @param sceneSetting The scene setting to parse.
 * @return The scene object.
 */
std::shared_ptr<Scene> Parser::parseScene(const libconfig::Setting &sceneSetting)
{
    if (_sceneNestingDepth >= 32)
        throw std::runtime_error("Scene nesting is too deep (maximum depth is 32)");

    rejectUnknownKeys(sceneSetting, "Nested scene",
                      {"id", "transform", "materials", "primitives", "lights", "scene"});

    NestingGuard nestingGuard(_sceneNestingDepth);
    MaterialCountGuard materialCountGuard(_materialCount);
    MtlIndexGuard mtlIndexGuard(_mtlMaterialIndices);
    SceneBuilder sceneBuilder;

    // Materials must be registered before primitives, because a nested scene
    // offsets its primitive material indices by the parent material count.
    if (sceneSetting.exists("materials"))
        parseMaterials(sceneBuilder, sceneSetting.lookup("materials"));

    if (sceneSetting.exists("primitives"))
        parsePrimitives(sceneBuilder, sceneSetting.lookup("primitives"));

    if (sceneSetting.exists("lights"))
        parseLights(sceneBuilder, sceneSetting.lookup("lights"));

    if (sceneSetting.exists("scene"))
    {
        const libconfig::Setting &nestedScene = sceneSetting.lookup("scene");

        rejectUnknownKeys(nestedScene, "Nested scene",
                          {"id", "transform", "materials", "primitives", "lights", "scene"});
        TransformPose pose;

        if (nestedScene.exists("transform"))
            pose = parseTransformPose(nestedScene.lookup("transform"));

        std::string instanceId;
        if (nestedScene.exists("id"))
        {
            instanceId = std::string(nestedScene.lookup("id").c_str());
            if (instanceId.empty())
                throw std::runtime_error("Nested scene id must not be empty");
        }

        sceneBuilder.addScene(parseScene(nestedScene), pose, instanceId);
        _materialCount = static_cast<uint>(sceneBuilder.getMaterialCount());
    }

    return sceneBuilder.build();
}

void Parser::parsePrefabs(const libconfig::Setting &prefabsSetting)
{
    PrefabFlagGuard prefabFlagGuard(_parsingPrefab);

    for (int i = 0; i < prefabsSetting.getLength(); ++i)
    {
        const libconfig::Setting &prefab = prefabsSetting[i];

        rejectUnknownKeys(prefab, std::string("Prefab '") + prefab.getName() + "'",
                          {"materials", "primitives", "lights", "scene"});
        const std::string name = prefab.getName();

        if (_prefabScenes.find(name) != _prefabScenes.end())
            throw std::runtime_error("Duplicate prefab: " + name);

        _prefabScenes[name] = parseScene(prefab);
    }
}

/**
 * Parses the sphere configuration from the configuration file.
 * Creates a sphere object with the radius, position, and material.
 *
 * @param sphereSetting The sphere setting to parse.
 * @return The sphere object.
 */
std::shared_ptr<IPrimitive> Parser::parseSphere(libconfig::Setting &sphereSetting)
{
    rejectUnknownKeys(sphereSetting, "Sphere", {"r", "position", "material", "transform"});
    const double radius = sphereSetting.lookup("r");

    requirePositive(radius, "Sphere radius");

    std::shared_ptr<IPrimitive> sphere =
        _factory.create<IPrimitive>("Sphere", radius, parsePoint(sphereSetting.lookup("position")),
                                    parseMaterialIndex(sphereSetting));

    return applyTransform(sphere, sphereSetting);
}

/**
 * Parses the plane configuration from the configuration file.
 * Creates a plane object with the position, width, height, and color.
 *
 * @param planeSetting The plane setting to parse.
 * @return The plane object.
 */
std::shared_ptr<IPrimitive> Parser::parsePlane(libconfig::Setting &planeSetting)
{
    rejectUnknownKeys(planeSetting, "Plane", {"normal", "position", "material", "transform"});
    const Vec3D normal = parseVector(planeSetting.lookup("normal"));

    requireNonZeroVector(normal, "Plane normal");

    std::shared_ptr<IPrimitive> plane =
        _factory.create<IPrimitive>("Plane", normal, parsePoint(planeSetting.lookup("position")),
                                    parseMaterialIndex(planeSetting));

    return applyTransform(plane, planeSetting);
}

/**
 * Parses the cylinder configuration from the configuration file.
 * Creates a cylinder object with the radius, center, height, and material.
 *
 * @param cylinderSetting The cylinder setting to parse.
 * @return The cylinder object.
 */
std::shared_ptr<IPrimitive> Parser::parseCylinder(libconfig::Setting &cylinderSetting)
{
    rejectUnknownKeys(cylinderSetting, "Cylinder", {"r", "height", "center", "material", "transform"});
    const double radius = cylinderSetting.lookup("r");
    const double height = cylinderSetting.lookup("height");

    requirePositive(radius, "Cylinder radius");
    requirePositive(height, "Cylinder height");

    std::shared_ptr<IPrimitive> cylinder =
        _factory.create<IPrimitive>("Cylinder", radius, parsePoint(cylinderSetting.lookup("center")), height,
                                    parseMaterialIndex(cylinderSetting));

    return applyTransform(cylinder, cylinderSetting);
}

/**
 * Parses the cone configuration from the configuration file.
 * Creates a cone object with the radius, center, height, and material.
 *
 * @param coneSetting The cone setting to parse.
 * @return The cone object.
 */
std::shared_ptr<IPrimitive> Parser::parseCone(libconfig::Setting &coneSetting)
{
    rejectUnknownKeys(coneSetting, "Cone", {"r", "h", "center", "material", "transform"});
    const double radius = coneSetting.lookup("r");
    const double height = coneSetting.lookup("h");

    requirePositive(radius, "Cone radius");
    requirePositive(height, "Cone height");

    std::shared_ptr<IPrimitive> cone =
        _factory.create<IPrimitive>("Cone", radius, parseVector(coneSetting.lookup("center")), height,
                                    parseMaterialIndex(coneSetting));

    return applyTransform(cone, coneSetting);
}

std::shared_ptr<IPrimitive> Parser::parseBox(libconfig::Setting &boxSetting)
{
    rejectUnknownKeys(boxSetting, "Box", {"min", "max", "material", "transform"});
    const Vec3D minimum = parseVector(boxSetting.lookup("min"));
    const Vec3D maximum = parseVector(boxSetting.lookup("max"));

    if (!(minimum.getX() < maximum.getX()) || !(minimum.getY() < maximum.getY()) ||
        !(minimum.getZ() < maximum.getZ()))
        throw std::runtime_error("Box minimum must be strictly below maximum on every axis");

    std::shared_ptr<IPrimitive> box =
        _factory.create<IPrimitive>("Box", minimum, maximum, parseMaterialIndex(boxSetting));

    return applyTransform(box, boxSetting);
}

std::shared_ptr<IPrimitive> Parser::parseDisk(libconfig::Setting &diskSetting)
{
    rejectUnknownKeys(diskSetting, "Disk", {"center", "normal", "radius", "innerRadius", "material", "transform"});
    const Vec3D center = parsePoint(diskSetting.lookup("center"));
    const Vec3D normal = parseVector(diskSetting.lookup("normal"));
    const double radius = diskSetting.lookup("radius");
    const double innerRadius = diskSetting.exists("innerRadius") ? diskSetting.lookup("innerRadius") : 0.0;

    requireNonZeroVector(normal, "Disk normal");
    requirePositive(radius, "Disk radius");
    requireNonNegative(innerRadius, "Disk inner radius");
    if (innerRadius >= radius)
        throw std::runtime_error("Disk inner radius must be smaller than its radius");

    std::shared_ptr<IPrimitive> disk =
        _factory.create<IPrimitive>("Disk", center, normal, radius, innerRadius, parseMaterialIndex(diskSetting));

    return applyTransform(disk, diskSetting);
}

std::shared_ptr<IPrimitive> Parser::applyTransform(std::shared_ptr<IPrimitive> primitive,
                                                   const libconfig::Setting &primitiveSetting)
{
    if (!primitive || !primitiveSetting.exists("transform"))
        return primitive;

    const TransformData transform = parseTransform(primitiveSetting.lookup("transform"));

    if (isIdentityTransform(transform))
        return primitive;

    return std::make_shared<InstancePrimitive>(primitive, transform);
}
