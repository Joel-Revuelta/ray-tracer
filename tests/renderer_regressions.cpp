/*
** EPITECH PROJECT, 2024
** raytracer
** File description:
** Regression tests for renderer/parser invariants
*/

#include "Factory.hpp"
#include "InstancePrimitive.hpp"
#include "Parser.hpp"
#include "Scene.hpp"
#include "SceneBuilder.hpp"
#include "TransformData.hpp"
#include "render/Environment.hpp"
#include "render/Texture.hpp"
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <limits>
#include <memory>
#include <stdexcept>
#include <string>

using namespace RayTracer;

namespace
{
int fail(const std::string &message)
{
    std::cerr << "renderer_regressions: " << message << std::endl;
    return 1;
}

bool closeTo(float actual, float expected, float tolerance = 1e-4f)
{
    return std::fabs(actual - expected) <= tolerance;
}

class NonCloneablePrimitive : public IPrimitive
{
  public:
    void doTransformation(const ITransformation &t) override
    {
        (void)t;
    }

    bool hit(const Ray &ray, HitPayload &payload, size_t index) const override
    {
        (void)ray;
        (void)payload;
        (void)index;
        return false;
    }

    uint getMaterialIndex(void) const override
    {
        return 0;
    }

    void setMaterialIndex(const uint &index) override
    {
        (void)index;
    }
};

int checkPhongCoefficients(const std::string &scenePath)
{
    Parser parser;
    std::shared_ptr<Scene> scene = parser.init(scenePath);

    if (!scene)
        return fail("could not parse the MTL regression scene");

    std::shared_ptr<IMaterial> redMaterial;

    for (const std::shared_ptr<IMaterial> &material : scene->getMaterials())
    {
        const Color3D color = material->getDiffuseColor();

        if (closeTo(color.getX(), 0.85f, 1e-3f) && closeTo(color.getY(), 0.12f, 1e-3f) &&
            closeTo(color.getZ(), 0.12f, 1e-3f))
        {
            redMaterial = material;
            break;
        }
    }

    if (!redMaterial)
        return fail("MTL red material was not imported");

    const std::shared_ptr<IMaterial> &material = redMaterial;

    if (!closeTo(material->getAmbientReflectionCoefficient(), 0.2f, 1e-3f))
        return fail("MTL Ka coefficient was not applied");
    if (!closeTo(material->getDiffuseReflectionCoefficient(), 1.0f, 1e-3f))
        return fail("MTL diffuse coefficient was not applied");
    if (!closeTo(material->getSpecularReflectionCoefficient(), 0.35f, 1e-3f))
        return fail("MTL Ks coefficient was not applied");

    const float expectedRoughness = std::sqrt(2.0f / (64.0f + 2.0f));

    if (!closeTo(material->getRoughness(), expectedRoughness, 1e-3f))
        return fail("MTL Ns roughness conversion was not applied");

    return 0;
}

int checkMaterialOverride(Factory &factory)
{
    const std::shared_ptr<IPrimitive> sphere =
        factory.create<IPrimitive>("Sphere", 1.0, Vec3D(0.0f, 0.0f, 0.0f), 0u);

    if (!sphere)
        return fail("could not create a sphere");

    const Ray ray(Vec3D(0.0f, 0.0f, -5.0f), Vec3D(0.0f, 0.0f, 1.0f));
    HitPayload payload;
    payload.setHitDistance(std::numeric_limits<float>::infinity());
    payload.setMaterialIndex(7);

    if (!sphere->hit(ray, payload, 0))
        return fail("sphere did not hit");
    if (payload.getMaterialIndex() != -1)
        return fail("non-mesh sphere kept a stale material override");

    const std::shared_ptr<IPrimitive> instance =
        std::make_shared<InstancePrimitive>(sphere, TransformData{});
    HitPayload instancePayload;
    instancePayload.setHitDistance(std::numeric_limits<float>::infinity());
    instancePayload.setMaterialIndex(7);

    if (!instance->hit(ray, instancePayload, 0))
        return fail("instance-wrapped sphere did not hit");
    if (instancePayload.getMaterialIndex() != -1)
        return fail("instance-wrapped non-mesh kept a stale material override");

    return 0;
}

int checkCloneRequirement(Factory &factory)
{
    SceneBuilder sourceBuilder;
    sourceBuilder.addMaterial(factory.create<IMaterial>("DiffuseMaterial", 0.2, 0.8, Color3D(1.0f, 1.0f, 1.0f)));
    sourceBuilder.addPrimitive(std::make_shared<NonCloneablePrimitive>());
    const std::shared_ptr<Scene> source = sourceBuilder.build();

    SceneBuilder destinationBuilder;
    destinationBuilder.addMaterial(
        factory.create<IMaterial>("DiffuseMaterial", 0.2, 0.8, Color3D(1.0f, 1.0f, 1.0f)));

    try
    {
        destinationBuilder.addScene(source);
    }
    catch (const std::runtime_error &)
    {
        return 0;
    }

    return fail("scene composition accepted a primitive without clone()");
}

int checkHDREnvironment(const std::string &path)
{
    const std::shared_ptr<const Texture> texture = loadTextureFile(path);

    if (!texture)
        return fail("could not load the HDR environment");

    float maximum = 0.0f;

    for (const Color3D &pixel : texture->getPixels())
    {
        maximum = std::max(maximum, pixel.getX());
        maximum = std::max(maximum, pixel.getY());
        maximum = std::max(maximum, pixel.getZ());
    }

    if (!(maximum > 1.0f) || !std::isfinite(maximum))
        return fail("HDR environment did not preserve values above 1.0");

    const std::shared_ptr<const EnvironmentDistribution> distribution = buildEnvironmentDistribution(*texture);

    if (!distribution || distribution->total <= 0.0f)
        return fail("HDR environment distribution is empty");

    uint32_t seed = 12345;

    for (int i = 0; i < 512; ++i)
    {
        const EnvironmentSample sample = sampleEnvironment(*texture, *distribution, seed);

        if (!sample.valid || !(sample.pdf > 0.0f) || !std::isfinite(sample.pdf))
            return fail("HDR environment sampling produced an invalid sample");
        if (!std::isfinite(sample.direction.lengthSquared()) ||
            std::fabs(sample.direction.length() - 1.0f) > 1e-3f)
            return fail("HDR environment sampling produced a non-unit direction");
        if (!std::isfinite(sample.radiance.getX()) || sample.radiance.getX() < 0.0f)
            return fail("HDR environment sampling produced invalid radiance");

        const float pdf = environmentPdf(*texture, *distribution, sample.direction);

        if (!(pdf > 0.0f) || !std::isfinite(pdf))
            return fail("HDR environment PDF is invalid");
    }

    return 0;
}

int checkTextureColorSpace(void)
{
    const char *path = "renderer_regressions_gray.ppm";
    std::ofstream file(path, std::ios::out | std::ios::binary);

    if (!file)
        return fail("could not create the temporary PPM");

    file << "P6\n1 1\n255\n";
    file.put(static_cast<char>(128));
    file.put(static_cast<char>(128));
    file.put(static_cast<char>(128));
    file.close();

    const std::shared_ptr<const Texture> texture = loadPPMTexture(path);

    if (!texture)
        return fail("could not load the temporary PPM");

    const float encoded = 128.0f / 255.0f;
    const float expected = std::pow((encoded + 0.055f) / 1.055f, 2.4f);

    if (!closeTo(texture->getPixels()[0].getX(), expected, 1e-4f))
        return fail("PPM texels were not decoded from sRGB to linear");

    return 0;
}
} // namespace

int main(int ac, char **av)
{
    if (ac != 3)
    {
        std::cerr << "usage: " << av[0] << " <mtl-scene> <hdr-environment>" << std::endl;
        return 2;
    }

    Factory factory;

    if (const int status = checkPhongCoefficients(av[1]); status != 0)
        return status;
    if (const int status = checkHDREnvironment(av[2]); status != 0)
        return status;
    if (const int status = checkMaterialOverride(factory); status != 0)
        return status;
    if (const int status = checkCloneRequirement(factory); status != 0)
        return status;
    if (const int status = checkTextureColorSpace(); status != 0)
        return status;

    std::cout << "renderer_regressions: all checks passed" << std::endl;
    return 0;
}
