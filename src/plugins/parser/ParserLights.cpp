/*
** EPITECH PROJECT, 2023
** raytracer
** File description:
** ParserLights.cpp
*/

#include "Parser.hpp"
#include "ParserInternal.hpp"
#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>
#include <string>

using namespace RayTracer;
using namespace RayTracer::ParserInternal;

/**
 * Parses the lights configuration from the configuration file.
 * Creates a light vector to store the light objects.
 *
 * @param sceneBuilder The scene builder to add the lights.
 * @param lightSetting The light setting to parse.
 */
void Parser::parseLights(SceneBuilder &sceneBuilder, const libconfig::Setting &lightSetting)
{
    rejectUnknownKeys(lightSetting, "Lights", {"ambient", "directional", "point", "spheres"});

    auto addLight = [&](std::shared_ptr<ILight> light, const libconfig::Setting &setting) {
        std::string id;

        if (setting.exists("id"))
        {
            id = std::string(setting.lookup("id").c_str());
            if (id.empty())
                throw std::runtime_error("Light id must not be empty");
            if (_parsingPrefab)
                throw std::runtime_error("Prefabs cannot contain named lights");
        }

        sceneBuilder.addLight(std::move(light), id);
    };

    if (lightSetting.exists("point"))
    {
        const libconfig::Setting &pointSetting = lightSetting.lookup("point");

        if (pointSetting.isList())
        {
            for (int i = 0; i < pointSetting.getLength(); i++)
                addLight(parsePointLight(pointSetting[i]), pointSetting[i]);
        }
        else
            addLight(parsePointLight(pointSetting), pointSetting);
    }

    if (lightSetting.exists("ambient"))
    {
        const libconfig::Setting &ambientSetting = lightSetting.lookup("ambient");

        addLight(parseAmbientLight(ambientSetting), ambientSetting);
    }

    if (lightSetting.exists("directional"))
    {
        const libconfig::Setting &directionalSetting = lightSetting.lookup("directional");

        addLight(parseDirectionalLight(directionalSetting), directionalSetting);
    }

    if (lightSetting.exists("spheres"))
    {
        const libconfig::Setting &sphereSetting = lightSetting.lookup("spheres");

        if (sphereSetting.isList())
        {
            for (int i = 0; i < sphereSetting.getLength(); i++)
                addLight(parseSphereLight(sphereSetting[i]), sphereSetting[i]);
        }
        else
            addLight(parseSphereLight(sphereSetting), sphereSetting);
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
    rejectUnknownKeys(directionalSetting, "Directional light",
                      {"position", "color", "intensity", "id"});

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
    rejectUnknownKeys(pointSetting, "Point light", {"position", "color", "intensity", "attenuation", "id"});

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
    rejectUnknownKeys(sphereSetting, "Sphere light",
                      {"position", "radius", "color", "intensity", "attenuation", "id"});

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

        rejectUnknownKeys(attenuation, "Sphere light attenuation", {"constant", "linear", "quadratic"});

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
    rejectUnknownKeys(ambientSetting, "Ambient light", {"color", "intensity", "id"});

    const double intensity = ambientSetting.lookup("intensity");

    requireNonNegative(intensity, "Ambient light intensity");

    std::shared_ptr<ILight> light =
        _factory.create<ILight>("AmbientLight", parseColor(ambientSetting.lookup("color")), intensity);

    return light;
}
