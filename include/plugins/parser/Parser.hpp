/*
** EPITECH PROJECT, 2023
** raytracer
** File description:
** Parser.hpp
*/

#ifndef PARSER_HPP_
#define PARSER_HPP_

#include "Camera.hpp"
#include "Color.hpp"
#include "Factory.hpp"
#include "IMaterial.hpp"
#include "Light.hpp"
#include "Primitive.hpp"
#include "Scene.hpp"
#include "SceneBuilder.hpp"
#include <libconfig.h++>
#include <unordered_map>

using MaterialVec = std::vector<std::shared_ptr<RayTracer::IMaterial>>;

namespace RayTracer
{

class MeshAsset;
class Texture;

class Parser
{
  public:
    Parser(void);
    ~Parser(void);

    std::shared_ptr<Scene> init(const std::string &filename);
    void initConfig(const std::string &filename);

    std::shared_ptr<Camera> parseCamera(const libconfig::Setting &cameraSetting);
    void parseMaterials(SceneBuilder &scene, const libconfig::Setting &primitiveSetting);

    void parseLights(SceneBuilder &scene, const libconfig::Setting &lightSetting);
    std::shared_ptr<ILight> parseDirectionalLight(const libconfig::Setting &directionalSetting);
    std::shared_ptr<ILight> parsePointLight(const libconfig::Setting &pointSetting);
    std::shared_ptr<ILight> parseSphereLight(const libconfig::Setting &sphereSetting);
    std::shared_ptr<ILight> parseAmbientLight(const libconfig::Setting &ambientSetting);

    void parsePrimitives(SceneBuilder &scene, libconfig::Setting &primitiveSetting);
    std::shared_ptr<Scene> parseScene(const libconfig::Setting &sceneSetting);
    std::shared_ptr<IPrimitive> parseSphere(libconfig::Setting &sphereSetting);
    std::shared_ptr<IPrimitive> parsePlane(libconfig::Setting &planeSetting);
    std::shared_ptr<IPrimitive> parseCone(libconfig::Setting &coneSetting);
    std::shared_ptr<IPrimitive> parseMesh(libconfig::Setting &meshSetting);
    std::shared_ptr<IPrimitive> parseCylinder(libconfig::Setting &cylinderSetting);

    Color3D parseColor(const libconfig::Setting &colorSetting);
    Point3D parsePoint(const libconfig::Setting &pointSetting);
    Vec3D parseVector(const libconfig::Setting &vectorSetting);
    TransformData parseTransform(const libconfig::Setting &transformSetting);
    Background parseBackground(const libconfig::Setting &backgroundSetting);

    std::shared_ptr<IPrimitive> applyTransform(std::shared_ptr<IPrimitive> primitive,
                                               const libconfig::Setting &primitiveSetting);

    std::shared_ptr<Camera> getParsedCamera(void) const;

  private:
    std::string resolveAssetPath(const std::string &path) const;
    std::shared_ptr<const MeshAsset> loadMesh(const std::string &path);
    std::shared_ptr<const Texture> loadTexture(const std::string &path);

    Factory _factory;
    std::shared_ptr<Camera> _camera;
    libconfig::Config _config;
    std::string _sceneDirectory = ".";
    std::unordered_map<std::string, std::shared_ptr<const MeshAsset>> _meshCache;
    std::unordered_map<std::string, std::shared_ptr<const Texture>> _textureCache;
    uint _materialCount = 0;
    int _sceneNestingDepth = 0;
};

} // namespace RayTracer
#endif // PARSER_HPP_
