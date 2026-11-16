/*
** EPITECH PROJECT, 2023
** raytracer
** File description:
** Parser.cpp
*/

#include "Parser.hpp"
#include "ParserInternal.hpp"
#include "InstancePrimitive.hpp"
#include "MeshLoader.hpp"
#include "render/Texture.hpp"
#include "Transform.hpp"
#include <algorithm>
#include <cmath>
#include <filesystem>
#include <iostream>
#include <limits>
#include <random>
#include <stdexcept>
#include <utility>

using namespace RayTracer;
using namespace RayTracer::ParserInternal;

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
    _animation.reset();
    _sceneAnimation.reset();
    _materialCount = 0;
    _mtlMaterialIndices.clear();
    _textureCache.clear();
    _meshCache.clear();
    _mtlCache.clear();
    _prefabScenes.clear();
    _parsingPrefab = false;

    try
    {
        initConfig(filename);
        rejectUnknownKeys(_config.getRoot(), "Scene",
                          {"camera", "animation", "materials", "prefabs", "primitives", "lights", "background",
                           "atmosphere"});
        _camera = parseCamera(_config.lookup("camera"));
        if (_config.exists("animation"))
            _animation = parseAnimation(_config.lookup("animation"));
        parseMaterials(sceneBuilder, _config.lookup("materials"));
        if (_config.exists("prefabs"))
            parsePrefabs(_config.lookup("prefabs"));
        parsePrimitives(sceneBuilder, _config.lookup("primitives"));
        parseLights(sceneBuilder, _config.lookup("lights"));
        if (_config.exists("background"))
            sceneBuilder.setBackground(parseBackground(_config.lookup("background")));
        if (_config.exists("atmosphere"))
            sceneBuilder.setAtmosphere(parseAtmosphere(_config.lookup("atmosphere")));

        std::shared_ptr<Scene> scene = sceneBuilder.build();

        if (_config.exists("animation") && scene)
            _sceneAnimation = parseSceneAnimation(_config.lookup("animation"), *scene);
        return scene;
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
    rejectUnknownKeys(cameraSetting, "Camera",
                      {"position", "rotation", "fieldOfView", "lensRadius", "focusDistance"});

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

std::string Parser::resolveAssetPath(const std::string &path) const
{
    const std::filesystem::path candidate(path);

    if (candidate.is_absolute())
        return candidate.lexically_normal().string();

    return (std::filesystem::path(_sceneDirectory) / candidate).lexically_normal().string();
}

std::shared_ptr<const Texture> Parser::loadTexture(const std::string &path, bool linear)
{
    const std::string cacheKey = linear ? path + "|linear" : path;
    const auto cached = _textureCache.find(cacheKey);

    if (cached != _textureCache.end())
        return cached->second;

    std::shared_ptr<const Texture> texture =
        loadTextureFile(path, linear ? TextureColorSpace::LINEAR : TextureColorSpace::SRGB);

    _textureCache.emplace(cacheKey, texture);
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
 * Parses an optional primitive transform into its human-readable components.
 * Rotation values are in degrees and the scale may be a scalar or a vector.
 *
 * @param transformSetting The transform setting to parse.
 * @return The transform pose.
 */
TransformPose Parser::parseTransformPose(const libconfig::Setting &transformSetting)
{
    rejectUnknownKeys(transformSetting, "Transform", {"translate", "rotate", "scale"});

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

    const TransformPose pose{translation, rotation, scale};

    if (!isInvertibleTransform(makeTransform(pose)))
        throw std::runtime_error("Transform is not invertible");

    return pose;
}

TransformData Parser::parseTransform(const libconfig::Setting &transformSetting)
{
    return makeTransform(parseTransformPose(transformSetting));
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

    requireNonNegative(r, "Color red");
    requireNonNegative(g, "Color green");
    requireNonNegative(b, "Color blue");
    return Color3D(r, g, b);
}

Background Parser::parseBackground(const libconfig::Setting &backgroundSetting)
{
    rejectUnknownKeys(backgroundSetting, "Background", {"texture", "zenith", "horizon", "ground", "intensity"});

    Background background;

    background.enabled = true;

    if (backgroundSetting.exists("texture"))
    {
        const std::string texturePath = resolveAssetPath(backgroundSetting.lookup("texture"));

        background.environment = loadTexture(texturePath);

        if (background.environment)
            background.environmentDistribution = buildEnvironmentDistribution(*background.environment);
    }

    if (backgroundSetting.exists("zenith"))
        background.zenith = parseColor(backgroundSetting.lookup("zenith"));
    if (backgroundSetting.exists("horizon"))
        background.horizon = parseColor(backgroundSetting.lookup("horizon"));
    if (backgroundSetting.exists("ground"))
        background.ground = parseColor(backgroundSetting.lookup("ground"));

    if (!background.environment &&
        (!backgroundSetting.exists("zenith") || !backgroundSetting.exists("horizon") ||
         !backgroundSetting.exists("ground")))
        throw std::runtime_error("Background requires a texture or zenith/horizon/ground colours");

    if (backgroundSetting.exists("intensity"))
    {
        const double intensity = backgroundSetting.lookup("intensity");

        requireNonNegative(intensity, "Background intensity");
        background.intensity = static_cast<float>(intensity);
    }

    return background;
}

Atmosphere Parser::parseAtmosphere(const libconfig::Setting &atmosphereSetting)
{
    rejectUnknownKeys(atmosphereSetting, "Atmosphere",
                      {"type", "density", "color", "anisotropy", "groundY", "heightFalloff", "maxDistance"});

    Atmosphere atmosphere;

    if (!atmosphereSetting.exists("type"))
        throw std::runtime_error("Atmosphere requires a type");

    const std::string type = atmosphereSetting.lookup("type");

    if (type == "homogeneous")
        atmosphere.type = AtmosphereType::HOMOGENEOUS;
    else if (type == "height")
        atmosphere.type = AtmosphereType::HEIGHT;
    else
        throw std::runtime_error("Unknown atmosphere type: " + type);

    const double density = settingAsDouble(atmosphereSetting.lookup("density"), "Atmosphere density");

    requireNonNegative(density, "Atmosphere density");
    atmosphere.density = static_cast<float>(density);

    if (atmosphereSetting.exists("color"))
        atmosphere.color = parseColor(atmosphereSetting.lookup("color"));

    const float components[3] = {atmosphere.color.getX(), atmosphere.color.getY(), atmosphere.color.getZ()};

    for (const float component : components)
        requireRange(component, 0.0, 1.0, "Atmosphere color component");

    if (atmosphereSetting.exists("anisotropy"))
    {
        const double anisotropy =
            settingAsDouble(atmosphereSetting.lookup("anisotropy"), "Atmosphere anisotropy");

        requireFinite(anisotropy, "Atmosphere anisotropy");
        if (anisotropy <= -1.0 || anisotropy >= 1.0)
            throw std::runtime_error("Atmosphere anisotropy must be strictly between -1 and 1");
        atmosphere.anisotropy = static_cast<float>(anisotropy);
    }

    if (atmosphereSetting.exists("maxDistance"))
    {
        const double maxDistance =
            settingAsDouble(atmosphereSetting.lookup("maxDistance"), "Atmosphere max distance");

        requirePositive(maxDistance, "Atmosphere max distance");
        atmosphere.maxDistance = static_cast<float>(maxDistance);
    }

    if (atmosphere.type == AtmosphereType::HEIGHT)
    {
        if (!atmosphereSetting.exists("groundY"))
            throw std::runtime_error("Height fog requires groundY");

        const double groundY = settingAsDouble(atmosphereSetting.lookup("groundY"), "Height fog ground");
        const double falloff =
            settingAsDouble(atmosphereSetting.lookup("heightFalloff"), "Height fog falloff");

        requireFinite(groundY, "Height fog ground");
        requirePositive(falloff, "Height fog falloff");
        atmosphere.groundY = static_cast<float>(groundY);
        atmosphere.heightFalloff = static_cast<float>(falloff);
    }

    return atmosphere;
}

std::shared_ptr<Camera> Parser::getParsedCamera(void) const
{
    return _camera;
}

std::shared_ptr<CameraAnimation> Parser::getParsedAnimation(void) const
{
    return _animation;
}

std::shared_ptr<SceneAnimation> Parser::getParsedSceneAnimation(void) const
{
    return _sceneAnimation;
}
