/*
** EPITECH PROJECT, 2023
** raytracer
** File description:
** Parser.cpp
*/

#include "Parser.hpp"

using namespace RayTracer;

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
    std::shared_ptr<Scene> scene;
    initConfig(filename);
    try
    {
        _camera = parseCamera(_config.lookup("camera"));
        parseMaterials(sceneBuilder, _config.lookup("materials"));
        parsePrimitives(sceneBuilder, _config.lookup("primitives"));
        parseLights(sceneBuilder, _config.lookup("lights"));
    }
    catch (const libconfig::SettingNotFoundException &e)
    {
        std::cout << "Any " << e.getPath() << " setting in configuration file." << std::endl;
    }
    catch (const libconfig::SettingTypeException &e)
    {
        std::cout << e.getPath() << " setting is not the right type." << std::endl;
    }
    scene = sceneBuilder.build();

    return scene;
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
    try
    {
        _config.readFile(filename.c_str());
    }
    catch (const libconfig::FileIOException &e)
    {
        std::cout << "Error while reading file." << std::endl;
    }
    catch (const libconfig::ParseException &e)
    {
        std::cout << "Parse error at " << e.getFile() << ":" << e.getLine() << " - " << e.getError() << std::endl;
    }
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

    return camera;
}

/**
 * Parses the material configuration from the configuration file.
 * Creates a material object with the material type and color.
 *
 * @param sceneBuilder The scene builder to add the materials.
 * @param primitiveSetting The primitive setting to parse.
 */
void Parser::parseMaterials(SceneBuilder sceneBuilder, const libconfig::Setting &materialSetting)
{
    for (int i = 0; i < materialSetting.getLength(); i++)
    {
        std::shared_ptr<IMaterial> material;
        libconfig::Setting &setting = materialSetting[i];

        if (std::string(setting.lookup("type")) == "diffuse")
        {
            material = _factory.create<IMaterial>("DiffuseMaterial", double(setting.lookup("ambientReflection")),
                                                  double(setting.lookup("diffuseReflection")),
                                                  parseColor(setting.lookup("color")));
        }
        else if (std::string(setting.lookup("type")) == "emissive")
        {
            material = _factory.create<IMaterial>("EmissiveMaterial", parseColor(setting.lookup("color")),
                                                  double(setting.lookup("intensity")),
                                                  parseColor(setting.lookup("emissionColor")));
        }
        else if (std::string(setting.lookup("type")) == "metallic")
        {
            material = _factory.create<IMaterial>("MetallicMaterial", parseColor(setting.lookup("color")));
        }
        else if (std::string(setting.lookup("type")) == "flatColor")
        {
            material = _factory.create<IMaterial>("FlatColorMaterial", parseColor(setting.lookup("color")));
        }
        else
        {
            material = _factory.create<IMaterial>(
                "CustomMaterial", double(setting.lookup("ambientReflection")),
                double(setting.lookup("diffuseReflection")), double(setting.lookup("specularReflection")),
                parseColor(setting.lookup("diffuseColor")), double(setting.lookup("emissionPower")),
                parseColor(setting.lookup("emissionColor")), parseMaterialType(setting.lookup("type")));
        }
        sceneBuilder.addMaterial(material);
    }
}

/**
 * Parses the lights configuration from the configuration file.
 * Creates a light vector to store the light objects.
 *
 * @param sceneBuilder The scene builder to add the lights.
 * @param lightSetting The light setting to parse.
 */
void Parser::parseLights(SceneBuilder sceneBuilder, const libconfig::Setting &lightSetting)
{
    for (int i = 0; lightSetting.exists("point") && i < lightSetting.lookup("point").getLength(); i++)
        sceneBuilder.addLight(parsePointLight(lightSetting.lookup("point")));

    if (lightSetting.exists("ambient"))
        sceneBuilder.addLight(parseAmbientLight(lightSetting.lookup("ambient")));

    if (lightSetting.exists("directional"))
        sceneBuilder.addLight(parseDirectionalLight(lightSetting.lookup("directional")));
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
    std::shared_ptr<ILight> light = _factory.create<ILight>("DirectionalLight", parsePoint(directionalSetting.lookup("position")),
                                                            parseColor(directionalSetting.lookup("color")),
                                                            double(directionalSetting.lookup("intensity")));

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
    std::shared_ptr<ILight> light = _factory.create<ILight>(
        "PointLight", parsePoint(pointSetting.lookup("position")), parseColor(pointSetting.lookup("color")),
        double(pointSetting.lookup("intensity")), double(pointSetting.lookup("attenuation.constant")),
        double(pointSetting.lookup("attenuation.linear")), double(pointSetting.lookup("attenuation.quadratic")));

    return light;
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
    std::shared_ptr<ILight> light = _factory.create<ILight>("AmbientLight", parseColor(ambientSetting.lookup("color")),
                                                            double(ambientSetting.lookup("intensity")));

    return light;
}

/**
 * Parses the primitives configuration from the configuration file.
 * ! Waiting for the cylinder class to be implemented
 *
 * @param sceneBuilder The scene builder to add the primitives.
 * @param primitiveSetting The primitive setting to parse.
 */
void Parser::parsePrimitives(SceneBuilder sceneBuilder, libconfig::Setting &primitiveSetting)
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
    SceneBuilder sceneBuilder;
    std::shared_ptr<Scene> scene;

    if (sceneSetting.exists("camera"))
        parseCamera(sceneSetting.lookup("camera"));
    
    if (sceneSetting.exists("lights"))
        parseLights(sceneBuilder, sceneSetting.lookup("lights"));
    
    if (sceneSetting.exists("primitives"))
        parsePrimitives(sceneBuilder, sceneSetting.lookup("primitives"));
    
    if (sceneSetting.exists("materials"))
        parseMaterials(sceneBuilder, sceneSetting.lookup("materials"));

    if (sceneSetting.exists("scene"))
        parseScene(sceneSetting.lookup("scene"));

    scene = sceneBuilder.build();

    return scene;
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
    std::shared_ptr<IPrimitive> sphere = _factory.create<IPrimitive>("Sphere", double(sphereSetting.lookup("r")),
                                                                     parsePoint(sphereSetting.lookup("position")),
                                                                     uint(sphereSetting.lookup("material")));

    return sphere;
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
    std::shared_ptr<IPrimitive> plane =
        _factory.create<IPrimitive>("Plane", parseVector(planeSetting.lookup("normal")),
                                    parsePoint(planeSetting.lookup("position")), uint(planeSetting.lookup("material")));

    return plane;
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
    std::shared_ptr<IPrimitive> cylinder = _factory.create<IPrimitive>(
        "Cylinder",
        double(cylinderSetting.lookup("r")),
        parsePoint(cylinderSetting.lookup("center")),
        double(cylinderSetting.lookup("height")),
        uint(cylinderSetting.lookup("material"))
    );

    return cylinder;
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
    std::shared_ptr<IPrimitive> cone = _factory.create<IPrimitive>(
        "Cone",
        double(coneSetting.lookup("r")),
        parseVector(coneSetting.lookup("center")),
        double(coneSetting.lookup("h")),
        uint(coneSetting.lookup("material"))
    );

    return cone;
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
    return Point3D(double(pointSetting.lookup("x")), double(pointSetting.lookup("y")),
                   double(pointSetting.lookup("z")));
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
    return Vec3D(double(vectorSetting.lookup("x")), double(vectorSetting.lookup("y")),
                 double(vectorSetting.lookup("z")));
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
    return Color3D(double(colorSetting.lookup("r")), double(colorSetting.lookup("g")),
                   double(colorSetting.lookup("b")));
}

/**
 * Parses the material type configuration from the configuration file.
 * Sets the material type.
 *
 * @param type The material type to parse.
 * @return The material type.
 */
IMaterial::MaterialType Parser::parseMaterialType(const std::string &type)
{
    if (type == "diffuse")
        return IMaterial::MaterialType::DIFFUSE;
    else if (type == "emissive")
        return IMaterial::MaterialType::EMISSIVE;
    else if (type == "metalic")
        return IMaterial::MaterialType::METALLIC;
    else if (type == "dielectric")
        return IMaterial::MaterialType::DIELECTRIC;
    else if (type == "flatColor")
        return IMaterial::MaterialType::FLAT_COLOR;
    else
        return IMaterial::MaterialType::TYPE_COUNT;
}

std::shared_ptr<Camera> Parser::getParsedCamera(void) const
{
    return _camera;
}
