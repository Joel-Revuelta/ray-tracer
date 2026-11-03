/*
** EPITECH PROJECT, 2023
** raytracer
** File description:
** Parser.cpp
*/

#include "Parser.hpp"
#include "InstancePrimitive.hpp"
#include "MeshLoader.hpp"
#include "render/Texture.hpp"
#include "Transform.hpp"
#include <filesystem>
#include <iostream>
#include <stdexcept>

using namespace RayTracer;

namespace
{
void requireFinite(double value, const std::string &name)
{
    if (!std::isfinite(value))
        throw std::runtime_error(name + " must be finite");
}

void requirePositive(double value, const std::string &name)
{
    requireFinite(value, name);
    if (value <= 0.0)
        throw std::runtime_error(name + " must be greater than zero");
}

void requireNonNegative(double value, const std::string &name)
{
    requireFinite(value, name);
    if (value < 0.0)
        throw std::runtime_error(name + " must not be negative");
}

void requireNonZeroVector(const Vec3D &vector, const std::string &name)
{
    if (!std::isfinite(vector.lengthSquared()) || vector.lengthSquared() < 1e-12f)
        throw std::runtime_error(name + " must be a non-zero vector");
}

void requireRange(double value, double minimum, double maximum, const std::string &name)
{
    requireFinite(value, name);
    if (value < minimum || value > maximum)
        throw std::runtime_error(name + " must be between " + std::to_string(minimum) + " and " +
                                 std::to_string(maximum));
}

uint parseMaterialIndex(const libconfig::Setting &setting)
{
    const int index = setting.lookup("material");

    if (index < 0)
        throw std::runtime_error("Material index must not be negative");
    return static_cast<uint>(index);
}

class NestingGuard
{
  public:
    explicit NestingGuard(int &depth) : _depth(depth)
    {
        ++_depth;
    }

    ~NestingGuard(void)
    {
        --_depth;
    }

    NestingGuard(const NestingGuard &) = delete;
    NestingGuard &operator=(const NestingGuard &) = delete;

  private:
    int &_depth;
};

class MaterialCountGuard
{
  public:
    explicit MaterialCountGuard(uint &count) : _count(count), _saved(count)
    {
        _count = 0;
    }

    ~MaterialCountGuard(void)
    {
        _count = _saved;
    }

    MaterialCountGuard(const MaterialCountGuard &) = delete;
    MaterialCountGuard &operator=(const MaterialCountGuard &) = delete;

  private:
    uint &_count;
    uint _saved;
};
} // namespace

Parser::Parser(void)
{
}

Parser::~Parser(void)
{
}

/**
 * Parses the configuration file and returns a shared pointer to the scene.
 * Calls the initConfig function to initialize the configuration file.
 * Builds the scene object adding the camera, lights, materials, primitives, and scene settings.
 * Catches exceptions if the setting is not found or if the setting is not the right type.
 *
 * @param filename The configuration file to parse.
 * @return The scene object.
 */
std::shared_ptr<Scene> Parser::init(const std::string &filename)
{
    SceneBuilder sceneBuilder;
    _camera.reset();
    _materialCount = 0;

    try
    {
        initConfig(filename);
        _camera = parseCamera(_config.lookup("camera"));
        parseMaterials(sceneBuilder, _config.lookup("materials"));
        parsePrimitives(sceneBuilder, _config.lookup("primitives"));
        parseLights(sceneBuilder, _config.lookup("lights"));
        if (_config.exists("background"))
            sceneBuilder.setBackground(parseBackground(_config.lookup("background")));
        return sceneBuilder.build();
    }
    catch (const libconfig::SettingNotFoundException &e)
    {
        std::cerr << "Missing setting in configuration file: " << e.getPath() << std::endl;
    }
    catch (const libconfig::SettingTypeException &e)
    {
        std::cerr << "Setting has the wrong type: " << e.getPath() << std::endl;
    }
    catch (const libconfig::FileIOException &)
    {
        std::cerr << "Error while reading configuration file: " << filename << std::endl;
    }
    catch (const libconfig::ParseException &e)
    {
        std::cerr << "Parse error at " << e.getFile() << ":" << e.getLine() << " - " << e.getError() << std::endl;
    }
    catch (const std::exception &e)
    {
        std::cerr << "Failed to parse scene: " << e.what() << std::endl;
    }

    return nullptr;
}

/**
 * Initializes the configuration file.
 * Creates a pointer to the configuration object
 * Sets the file to read and returns the configuration object.
 * Catches exceptions if the file cannot be read or if there is a parse error.
 *
 * @param filename The configuration file to initialize the parser.
 */
void Parser::initConfig(const std::string &filename)
{
    _config.readFile(filename.c_str());

    const std::filesystem::path scenePath(filename);

    _sceneDirectory = scenePath.parent_path().empty() ? "." : scenePath.parent_path().string();
}

/**
 * Parses the camera configuration from the configuration file.
 * Creates a camera object with the resolution, position, rotation, and field of view.
 *
 * @param cameraSetting The camera setting to parse.
 * @return The camera object.
 */
std::shared_ptr<Camera> Parser::parseCamera(const libconfig::Setting &cameraSetting)
{
    std::shared_ptr<Camera> camera = std::make_shared<Camera>(parsePoint(cameraSetting.lookup("rotation")),
                                                              parsePoint(cameraSetting.lookup("position")),
                                                              double(cameraSetting.lookup("fieldOfView")));

    if (cameraSetting.exists("lensRadius"))
    {
        const double lensRadius = cameraSetting.lookup("lensRadius");

        requireNonNegative(lensRadius, "Camera lens radius");
        camera->setLensRadius(static_cast<float>(lensRadius));
    }

    if (cameraSetting.exists("focusDistance"))
    {
        const double focusDistance = cameraSetting.lookup("focusDistance");

        requirePositive(focusDistance, "Camera focus distance");
        camera->setFocusDistance(static_cast<float>(focusDistance));
    }

    return camera;
}

/**
 * Parses the material configuration from the configuration file.
 * Creates a material object with the material type and color.
 *
 * @param sceneBuilder The scene builder to add the materials.
 * @param primitiveSetting The primitive setting to parse.
 */
void Parser::parseMaterials(SceneBuilder &sceneBuilder, const libconfig::Setting &materialSetting)
{
    for (int i = 0; i < materialSetting.getLength(); i++)
    {
        std::shared_ptr<IMaterial> material;
        libconfig::Setting &setting = materialSetting[i];
        const std::string type = setting.lookup("type");

        if (type == "diffuse")
        {
            const double ambientReflection = setting.lookup("ambientReflection");
            const double diffuseReflection = setting.lookup("diffuseReflection");

            requireNonNegative(ambientReflection, "Diffuse material ambient reflection");
            requireNonNegative(diffuseReflection, "Diffuse material diffuse reflection");
            material = _factory.create<IMaterial>("DiffuseMaterial", ambientReflection, diffuseReflection,
                                                  parseColor(setting.lookup("color")));
        }
        else if (type == "emissive")
        {
            const double intensity = setting.lookup("intensity");

            requireNonNegative(intensity, "Emissive material intensity");
            material = _factory.create<IMaterial>("EmissiveMaterial", parseColor(setting.lookup("color")), intensity,
                                                  parseColor(setting.lookup("emissionColor")));
        }
        else if (type == "metallic" || type == "metalic")
        {
            const double roughness = setting.exists("roughness") ? double(setting.lookup("roughness")) : 0.0;

            requireRange(roughness, 0.0, 1.0, "Metallic material roughness");
            material = _factory.create<IMaterial>("MetallicMaterial", parseColor(setting.lookup("color")), roughness);
        }
        else if (type == "dielectric")
        {
            const double ior = setting.exists("ior") ? double(setting.lookup("ior")) : 1.5;

            requirePositive(ior, "Dielectric material IOR");
            material = _factory.create<IMaterial>("DielectricMaterial", parseColor(setting.lookup("color")), ior);
        }
        else if (type == "flatColor")
        {
            material = _factory.create<IMaterial>("FlatColorMaterial", parseColor(setting.lookup("color")));
        }
        else
        {
            throw std::runtime_error("Unknown material type: " + type);
        }

        if (setting.exists("texture"))
        {
            const std::string texturePath = resolveAssetPath(setting.lookup("texture"));

            material->setTexture(loadTexture(texturePath));
        }

        sceneBuilder.addMaterial(material);
        ++_materialCount;
    }
}

/**
 * Parses the lights configuration from the configuration file.
 * Creates a light vector to store the light objects.
 *
 * @param sceneBuilder The scene builder to add the lights.
 * @param lightSetting The light setting to parse.
 */
void Parser::parseLights(SceneBuilder &sceneBuilder, const libconfig::Setting &lightSetting)
{
    if (lightSetting.exists("point"))
    {
        const libconfig::Setting &pointSetting = lightSetting.lookup("point");

        if (pointSetting.isList())
        {
            for (int i = 0; i < pointSetting.getLength(); i++)
                sceneBuilder.addLight(parsePointLight(pointSetting[i]));
        }
        else
            sceneBuilder.addLight(parsePointLight(pointSetting));
    }

    if (lightSetting.exists("ambient"))
        sceneBuilder.addLight(parseAmbientLight(lightSetting.lookup("ambient")));

    if (lightSetting.exists("directional"))
        sceneBuilder.addLight(parseDirectionalLight(lightSetting.lookup("directional")));

    if (lightSetting.exists("spheres"))
    {
        const libconfig::Setting &sphereSetting = lightSetting.lookup("spheres");

        if (sphereSetting.isList())
        {
            for (int i = 0; i < sphereSetting.getLength(); i++)
                sceneBuilder.addLight(parseSphereLight(sphereSetting[i]));
        }
        else
            sceneBuilder.addLight(parseSphereLight(sphereSetting));
    }
}

/**
 * Parses the light configuration from the configuration file.
 * Creates a light object with the color and intensity.
 *
 * @param directionalSetting The directional light setting to parse.
 * @return The light object.
 */
std::shared_ptr<ILight> Parser::parseDirectionalLight(const libconfig::Setting &directionalSetting)
{
    const Vec3D direction = parsePoint(directionalSetting.lookup("position"));
    const double intensity = directionalSetting.lookup("intensity");

    requireNonZeroVector(direction, "Directional light direction");
    requireNonNegative(intensity, "Directional light intensity");

    std::shared_ptr<ILight> light =
        _factory.create<ILight>("DirectionalLight", direction, parseColor(directionalSetting.lookup("color")), intensity);

    return light;
}

/**
 * Parses the point light configuration from the configuration file.
 * Creates a light object with the color and intensity.
 *
 * @param pointSetting The point light setting to parse.
 * @return The light object.
 */
std::shared_ptr<ILight> Parser::parsePointLight(const libconfig::Setting &pointSetting)
{
    const double intensity = pointSetting.lookup("intensity");
    const double attenuationConstant = pointSetting.lookup("attenuation.constant");
    const double attenuationLinear = pointSetting.lookup("attenuation.linear");
    const double attenuationQuadratic = pointSetting.lookup("attenuation.quadratic");

    requireNonNegative(intensity, "Point light intensity");
    requirePositive(attenuationConstant, "Point light attenuation constant");
    requireNonNegative(attenuationLinear, "Point light attenuation linear");
    requireNonNegative(attenuationQuadratic, "Point light attenuation quadratic");

    std::shared_ptr<ILight> light = _factory.create<ILight>(
        "PointLight", parsePoint(pointSetting.lookup("position")), parseColor(pointSetting.lookup("color")), intensity,
        attenuationConstant, attenuationLinear, attenuationQuadratic);

    return light;
}

/**
 * Parses a spherical area light. The radius controls the softness of the
 * shadows and the attenuation follows the point-light model.
 *
 * @param sphereSetting The sphere light setting to parse.
 * @return The light object.
 */
std::shared_ptr<ILight> Parser::parseSphereLight(const libconfig::Setting &sphereSetting)
{
    const Vec3D position = parsePoint(sphereSetting.lookup("position"));
    const double radius = sphereSetting.lookup("radius");
    const double intensity = sphereSetting.lookup("intensity");
    const Color3D color = parseColor(sphereSetting.lookup("color"));

    requirePositive(radius, "Sphere light radius");
    requireNonNegative(intensity, "Sphere light intensity");

    double attenuationConstant = 1.0;
    double attenuationLinear = 0.0;
    double attenuationQuadratic = 0.0;

    if (sphereSetting.exists("attenuation"))
    {
        const libconfig::Setting &attenuation = sphereSetting.lookup("attenuation");

        attenuationConstant = attenuation.lookup("constant");
        attenuationLinear = attenuation.lookup("linear");
        attenuationQuadratic = attenuation.lookup("quadratic");
    }

    requirePositive(attenuationConstant, "Sphere light attenuation constant");
    requireNonNegative(attenuationLinear, "Sphere light attenuation linear");
    requireNonNegative(attenuationQuadratic, "Sphere light attenuation quadratic");

    return _factory.create<ILight>("SphereLight", position, radius, color, intensity, attenuationConstant,
                                   attenuationLinear, attenuationQuadratic);
}

/**
 * Parses the ambient light configuration from the configuration file.
 * Creates an light object with the color and intensity.
 *
 * @param ambientSetting The ambient light setting to parse.
 * @return The light object.
 */
std::shared_ptr<ILight> Parser::parseAmbientLight(const libconfig::Setting &ambientSetting)
{
    const double intensity = ambientSetting.lookup("intensity");

    requireNonNegative(intensity, "Ambient light intensity");

    std::shared_ptr<ILight> light =
        _factory.create<ILight>("AmbientLight", parseColor(ambientSetting.lookup("color")), intensity);

    return light;
}

/**
 * Parses the primitives configuration from the configuration file.
 * ! Waiting for the cylinder class to be implemented
 *
 * @param sceneBuilder The scene builder to add the primitives.
 * @param primitiveSetting The primitive setting to parse.
 */
void Parser::parsePrimitives(SceneBuilder &sceneBuilder, libconfig::Setting &primitiveSetting)
{
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

    for (int i = 0; primitiveSetting.exists("meshes")
        && i < primitiveSetting.lookup("meshes").getLength(); i++) {
        sceneBuilder.addPrimitive(parseMesh(primitiveSetting.lookup("meshes")[i]));
    }

    if (primitiveSetting.exists("scene")) {
        sceneBuilder.addScene(parseScene(primitiveSetting.lookup("scene")));
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

    NestingGuard nestingGuard(_sceneNestingDepth);
    MaterialCountGuard materialCountGuard(_materialCount);
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
        sceneBuilder.addScene(parseScene(sceneSetting.lookup("scene")));

    return sceneBuilder.build();
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
    const double radius = coneSetting.lookup("r");
    const double height = coneSetting.lookup("h");

    requirePositive(radius, "Cone radius");
    requirePositive(height, "Cone height");

    std::shared_ptr<IPrimitive> cone =
        _factory.create<IPrimitive>("Cone", radius, parseVector(coneSetting.lookup("center")), height,
                                    parseMaterialIndex(coneSetting));

    return applyTransform(cone, coneSetting);
}

std::string Parser::resolveAssetPath(const std::string &path) const
{
    const std::filesystem::path candidate(path);

    if (candidate.is_absolute())
        return candidate.lexically_normal().string();

    return (std::filesystem::path(_sceneDirectory) / candidate).lexically_normal().string();
}

std::shared_ptr<const Texture> Parser::loadTexture(const std::string &path)
{
    const auto cached = _textureCache.find(path);

    if (cached != _textureCache.end())
        return cached->second;

    std::shared_ptr<const Texture> texture = loadPPMTexture(path);

    _textureCache.emplace(path, texture);
    return texture;
}

std::shared_ptr<const MeshAsset> Parser::loadMesh(const std::string &path)
{
    const auto cached = _meshCache.find(path);

    if (cached != _meshCache.end())
        return cached->second;

    std::shared_ptr<const MeshAsset> asset = loadObjMesh(path);

    _meshCache.emplace(path, asset);
    return asset;
}

/**
 * Parses a mesh primitive. The mesh asset is loaded once per resolved path
 * and shared by every instance.
 *
 * @param meshSetting The mesh entry to parse.
 * @return The mesh primitive, optionally wrapped in a transform.
 */
std::shared_ptr<IPrimitive> Parser::parseMesh(libconfig::Setting &meshSetting)
{
    const std::string path = resolveAssetPath(meshSetting.lookup("path"));
    const std::shared_ptr<const MeshAsset> asset = loadMesh(path);
    std::vector<std::pair<std::string, uint>> materialGroups;

    if (meshSetting.exists("materialGroups"))
    {
        const libconfig::Setting &groupSetting = meshSetting.lookup("materialGroups");

        for (int i = 0; i < groupSetting.getLength(); ++i)
        {
            const libconfig::Setting &group = groupSetting[i];
            const std::string name = group.getName();
            const int materialIndex = static_cast<int>(group);

            if (materialIndex < 0 || static_cast<uint>(materialIndex) >= _materialCount)
                throw std::runtime_error("Material group '" + name + "' references an invalid material index");

            materialGroups.emplace_back(name, static_cast<uint>(materialIndex));
        }
    }

    std::shared_ptr<IPrimitive> mesh =
        _factory.create<IPrimitive>("Mesh", asset, parseMaterialIndex(meshSetting), materialGroups);

    return applyTransform(mesh, meshSetting);
}

/**
 * Parses the 3D point configuration from the configuration file.
 * Sets the point's x, y, and z values.
 *
 * @param pointSetting The point setting to parse.
 * @return The point object.
 */
Point3D Parser::parsePoint(const libconfig::Setting &pointSetting)
{
    const double x = pointSetting.lookup("x");
    const double y = pointSetting.lookup("y");
    const double z = pointSetting.lookup("z");

    requireFinite(x, "Point x");
    requireFinite(y, "Point y");
    requireFinite(z, "Point z");
    return Point3D(x, y, z);
}

/**
 * Parses the 3D vector configuration from the configuration file.
 * Sets the vector's x, y, and z values.
 *
 * @param vectorSetting The vector setting to parse.
 * @return The vector object.
 */
Vec3D Parser::parseVector(const libconfig::Setting &vectorSetting)
{
    const double x = vectorSetting.lookup("x");
    const double y = vectorSetting.lookup("y");
    const double z = vectorSetting.lookup("z");

    requireFinite(x, "Vector x");
    requireFinite(y, "Vector y");
    requireFinite(z, "Vector z");
    return Vec3D(x, y, z);
}

/**
 * Parses an optional primitive transform.
 * Rotation values are in degrees and the scale may be a scalar or a vector.
 *
 * @param transformSetting The transform setting to parse.
 * @return The composed local-to-world transform.
 */
TransformData Parser::parseTransform(const libconfig::Setting &transformSetting)
{
    Vec3D translation(0.0f);
    Vec3D rotation(0.0f);
    Vec3D scale(1.0f);

    if (transformSetting.exists("translate"))
        translation = parseVector(transformSetting.lookup("translate"));

    if (transformSetting.exists("rotate"))
        rotation = parseVector(transformSetting.lookup("rotate"));

    if (transformSetting.exists("scale"))
    {
        const libconfig::Setting &scaleSetting = transformSetting.lookup("scale");

        if (scaleSetting.isNumber())
        {
            const double value = double(scaleSetting);

            requireFinite(value, "Transform scale");
            scale = Vec3D(static_cast<float>(value));
        }
        else
            scale = parseVector(scaleSetting);
    }

    if (scale.getX() == 0.0f || scale.getY() == 0.0f || scale.getZ() == 0.0f)
        throw std::runtime_error("Transform scale components must be non-zero");

    const TransformData transform = makeTransform(translation, rotation, scale);

    if (!isInvertibleTransform(transform))
        throw std::runtime_error("Transform is not invertible");

    return transform;
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

/**
 * Parses the color configuration from the configuration file.
 * Sets the color's red, green, and blue values.
 *
 * @param colorSetting The color setting to parse.
 * @return The color object.
 */
Color3D Parser::parseColor(const libconfig::Setting &colorSetting)
{
    const double r = colorSetting.lookup("r");
    const double g = colorSetting.lookup("g");
    const double b = colorSetting.lookup("b");

    requireFinite(r, "Color red");
    requireFinite(g, "Color green");
    requireFinite(b, "Color blue");
    return Color3D(r, g, b);
}

Background Parser::parseBackground(const libconfig::Setting &backgroundSetting)
{
    Background background;

    background.enabled = true;
    background.zenith = parseColor(backgroundSetting.lookup("zenith"));
    background.horizon = parseColor(backgroundSetting.lookup("horizon"));
    background.ground = parseColor(backgroundSetting.lookup("ground"));

    if (backgroundSetting.exists("intensity"))
    {
        const double intensity = backgroundSetting.lookup("intensity");

        requireNonNegative(intensity, "Background intensity");
        background.intensity = static_cast<float>(intensity);
    }

    return background;
}

std::shared_ptr<Camera> Parser::getParsedCamera(void) const
{
    return _camera;
}
