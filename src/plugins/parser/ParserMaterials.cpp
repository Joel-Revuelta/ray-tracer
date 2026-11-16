/*
** EPITECH PROJECT, 2023
** raytracer
** File description:
** ParserMaterials.cpp
*/

#include "Parser.hpp"
#include "ParserInternal.hpp"
#include "MeshLoader.hpp"
#include "render/Texture.hpp"
#include <algorithm>
#include <filesystem>
#include <limits>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <utility>

using namespace RayTracer;
using namespace RayTracer::ParserInternal;

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

        rejectUnknownKeys(setting, std::string("Material '") + setting.getName() + "'",
                          {"type", "ambientReflection", "diffuseReflection", "roughness", "ior", "color",
                           "emissionColor", "intensity", "texture", "normalTexture", "roughnessTexture",
                           "emissionTexture"});

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
        if (setting.exists("normalTexture"))
        {
            const std::string texturePath = resolveAssetPath(setting.lookup("normalTexture"));

            material->setNormalTexture(loadTexture(texturePath, true));
        }
        if (setting.exists("roughnessTexture"))
        {
            const std::string texturePath = resolveAssetPath(setting.lookup("roughnessTexture"));

            material->setRoughnessTexture(loadTexture(texturePath, true));
        }
        if (setting.exists("emissionTexture"))
        {
            const std::string texturePath = resolveAssetPath(setting.lookup("emissionTexture"));

            material->setEmissionTexture(loadTexture(texturePath));
        }

        sceneBuilder.addMaterial(material);
        ++_materialCount;
    }
}

/**
 * Parses a mesh primitive. The mesh asset is loaded once per resolved path
 * and shared by every instance.
 *
 * @param meshSetting The mesh entry to parse.
 * @return The mesh primitive, optionally wrapped in a transform.
 */
std::shared_ptr<IPrimitive> Parser::parseMesh(SceneBuilder &sceneBuilder, libconfig::Setting &meshSetting)
{
    rejectUnknownKeys(meshSetting, "Mesh", {"path", "material", "materialGroups", "transform"});

    const std::string path = resolveAssetPath(meshSetting.lookup("path"));
    const std::shared_ptr<const MeshAsset> asset = loadMesh(path);
    const uint fallbackMaterial = parseMaterialIndex(meshSetting);
    std::unordered_map<std::string, uint> groupMap;

    if (meshSetting.exists("materialGroups"))
    {
        const libconfig::Setting &groupSetting = meshSetting.lookup("materialGroups");

        for (int i = 0; i < groupSetting.getLength(); ++i)
        {
            const libconfig::Setting &group = groupSetting[i];
            const std::string name = group.getName();
            const int materialIndex = settingAsInt(group, "Material group '" + name + "' index", 0,
                                                   static_cast<int>(std::numeric_limits<int>::max()));

            if (static_cast<uint>(materialIndex) >= _materialCount)
                throw std::runtime_error("Material group '" + name + "' references an invalid material index");

            groupMap[name] = static_cast<uint>(materialIndex);
        }
    }

    std::unordered_map<std::string, uint> autoGroups;

    for (const std::string &libraryPath : asset->materialLibraries)
    {
        auto cached = _mtlCache.find(libraryPath);

        if (cached == _mtlCache.end())
        {
            std::vector<MtlMaterial> materials = loadMtl(libraryPath);

            cached = _mtlCache.emplace(libraryPath, std::move(materials)).first;
        }

        for (const MtlMaterial &mtl : cached->second)
        {
            const std::string key = libraryPath + '\n' + mtl.name;
            const auto existing = _mtlMaterialIndices.find(key);
            uint materialIndex = 0;

            if (existing != _mtlMaterialIndices.end())
                materialIndex = existing->second;
            else
            {
                const float ambient = std::clamp(luminance(mtl.ambient), 0.0f, 1.0f);
                const float specular = std::clamp(luminance(mtl.specular), 0.0f, 1.0f);

                // The plugin ABI is untyped at the dlopen boundary, so the
                // call-site argument types must match the exported signature
                // exactly. PhongMaterial::create takes doubles; passing floats
                // here silently corrupts the coefficients.
                std::shared_ptr<IMaterial> material = _factory.create<IMaterial, double, double, double, Color3D, double>(
                    "PhongMaterial", static_cast<double>(ambient), 1.0, static_cast<double>(specular), mtl.diffuse,
                    static_cast<double>(roughnessFromShininess(mtl.shininess)));

                if (!mtl.diffuseMap.empty())
                    material->setTexture(loadTexture(mtl.diffuseMap));
                if (!mtl.specularMap.empty())
                    material->setRoughnessTexture(loadTexture(mtl.specularMap, true));

                const bool hasEmission = luminance(mtl.emission) > 0.0f;

                if (hasEmission)
                {
                    material->setEmissionColor(mtl.emission);
                    material->setEmissionPower(1.0f);
                }
                if (!mtl.emissionMap.empty())
                {
                    // MTL Ke and map_Ke are conventionally additive. When no
                    // scalar Ke is present, let a white emitter drive the map.
                    if (!hasEmission)
                    {
                        material->setEmissionColor(Color3D(1.0f, 1.0f, 1.0f));
                        material->setEmissionPower(1.0f);
                    }
                    material->setEmissionTexture(loadTexture(mtl.emissionMap));
                }

                sceneBuilder.addMaterial(material);
                materialIndex = _materialCount++;
                _mtlMaterialIndices.emplace(key, materialIndex);
            }

            autoGroups[mtl.name] = materialIndex;
        }
    }

    for (const auto &entry : autoGroups)
    {
        if (groupMap.find(entry.first) == groupMap.end())
            groupMap.emplace(entry.first, entry.second);
    }

    std::vector<std::pair<std::string, uint>> materialGroups(groupMap.begin(), groupMap.end());

    std::sort(materialGroups.begin(), materialGroups.end(),
              [](const auto &left, const auto &right) { return left.first < right.first; });

    std::shared_ptr<IPrimitive> mesh =
        _factory.create<IPrimitive>("Mesh", asset, fallbackMaterial, materialGroups);

    return applyTransform(mesh, meshSetting);
}
