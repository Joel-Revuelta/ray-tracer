/*
** EPITECH PROJECT, 2024
** raytracer
** File description:
** Regression tests for renderer/parser invariants
*/

#include "Factory.hpp"
#include "InstancePrimitive.hpp"
#include "Parser.hpp"
#include "plugins/renderers/CameraController.hpp"
#include "Scene.hpp"
#include "SceneBuilder.hpp"
#include "MeshLoader.hpp"
#include "TransformData.hpp"
#include "render/Animation.hpp"
#include "render/AtmosphereSampling.hpp"
#include "render/DielectricStack.hpp"
#include "render/Environment.hpp"
#include "plugins/renderers/PPM.hpp"
#include "render/ImageWriter.hpp"
#include "render/SceneAnimation.hpp"
#include "render/TemporaryPath.hpp"
#include "render/Texture.hpp"
#include "render/VideoWriter.hpp"
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <filesystem>
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

int checkMtlMaps(const std::string &scenePath)
{
    Parser parser;
    std::shared_ptr<Scene> scene = parser.init(scenePath);

    if (!scene)
        return fail("could not parse the MTL map regression scene");

    bool checkedRoughness = false;
    bool checkedEmission = false;

    for (const std::shared_ptr<IMaterial> &material : scene->getMaterials())
    {
        const Color3D color = material->getDiffuseColor();

        if (closeTo(color.getX(), 0.32f, 1e-3f) && closeTo(color.getY(), 0.24f, 1e-3f) &&
            closeTo(color.getZ(), 0.16f, 1e-3f))
        {
            if (!material->getRoughnessTexture())
                return fail("MTL map_Ks was not imported as a roughness texture");
            if (material->getEmissionTexture())
                return fail("MTL brushed material unexpectedly has an emission map");
            checkedRoughness = true;
        }

        if (closeTo(color.getX(), 0.15f, 1e-3f) && closeTo(color.getY(), 0.18f, 1e-3f) &&
            closeTo(color.getZ(), 0.25f, 1e-3f))
        {
            if (!material->getEmissionTexture())
                return fail("MTL map_Ke was not imported as an emission texture");
            if (!(material->getEmissionColor().lengthSquared() > 0.0f))
                return fail("MTL Ke was not applied as a constant emission term");
            checkedEmission = true;
        }
    }

    if (!checkedRoughness)
        return fail("MTL map_Ks material was not imported");
    if (!checkedEmission)
        return fail("MTL map_Ke material was not imported");
    return 0;
}

int checkSelfHitEpsilon(Factory &factory)
{
    const std::shared_ptr<IPrimitive> plane =
        factory.create<IPrimitive>("Plane", Vec3D(0.0f, 1.0f, 0.0f), Vec3D(0.0f), 0u);
    const Ray planeRay(Vec3D(0.0f), Vec3D(0.0f, 1.0f, 1.0f));

    HitPayload planePayload;

    if (plane->hit(planeRay, planePayload, 0))
        return fail("plane accepted a zero-distance self-hit");

    const std::shared_ptr<IPrimitive> cylinder =
        factory.create<IPrimitive>("Cylinder", 1.0, Vec3D(0.0f), 2.0, 0u);
    const Ray cylinderRay(Vec3D(1.0f, 1.0f, 0.0f), Vec3D(1.0f, 0.0f, 0.0f));

    HitPayload cylinderPayload;

    if (cylinder->hit(cylinderRay, cylinderPayload, 0))
        return fail("cylinder accepted a zero-distance self-hit");

    const std::shared_ptr<IPrimitive> cone =
        factory.create<IPrimitive>("Cone", 1.0, Vec3D(0.0f), 2.0, 0u);
    const Ray coneRay(Vec3D(1.0f, -2.0f, 0.0f), Vec3D(1.0f, 0.0f, 0.0f));

    HitPayload conePayload;

    if (cone->hit(coneRay, conePayload, 0))
        return fail("cone accepted a zero-distance self-hit");

    return 0;
}

int checkConcaveMesh(void)
{
    const char *path = "renderer_regressions_concave.obj";
    std::ofstream file(path, std::ios::out | std::ios::binary);

    if (!file)
        return fail("could not create the concave OBJ");

    file << "o concave\n";
    file << "v 0 0 0\nv 3 0 0\nv 3 3 0\nv 2 3 0\n";
    file << "v 2 1 0\nv 1 1 0\nv 1 3 0\nv 0 3 0\n";
    file << "f 1 2 3 4 5 6 7 8\n";
    file.close();

    const std::shared_ptr<const MeshAsset> asset = loadObjMesh(path);

    if (!asset || asset->triangles.size() != 6)
        return fail("concave polygon was not triangulated into n-2 triangles");

    float area = 0.0f;

    for (const Triangle &triangle : asset->triangles)
        area += 0.5f * vec3DCross(triangle.v1 - triangle.v0, triangle.v2 - triangle.v0).length();

    if (std::fabs(area - 7.0f) > 1e-3f)
        return fail("concave polygon triangulation has the wrong area");

    return 0;
}

int checkVectorAccess(void)
{
    const Vec3D vector(1.0f, 2.0f, 3.0f);

    if (!closeTo(vector.at(0), 1.0f) || !closeTo(vector.at(1), 2.0f) || !closeTo(vector.at(2), 3.0f))
        return fail("checked vector access returned the wrong component");

    try
    {
        (void)vector.at(3);
        return fail("checked vector access accepted an out-of-range index");
    }
    catch (const VectorIndexOutOfBounds &)
    {
    }

    return 0;
}

int checkPPMReader(void)
{
    const std::string binaryPath = makeTemporaryPath("renderer_regressions-ppm.ppm");

    {
        std::ofstream file(binaryPath, std::ios::binary);

        if (!file)
            return fail("could not create a binary PPM fixture");

        file << "P6\n# header comment\n2 1\n255\n";
        const unsigned char data[] = {1, 2, 3, 250, 251, 252};

        file.write(reinterpret_cast<const char *>(data), sizeof(data));
    }

    const PPMImage binary = readPPMFile(binaryPath);

    std::filesystem::remove(binaryPath);

    if (binary.width != 2 || binary.height != 1 || binary.pixels.size() != 2)
        return fail("PPM reader returned the wrong binary image shape");
    if (binary.pixels[0] != 0xFF030201u || binary.pixels[1] != 0xFFFCFBFAu)
        return fail("PPM reader returned the wrong binary pixel layout");

    const std::string asciiPath = makeTemporaryPath("renderer_regressions-ppm-ascii.ppm");

    {
        std::ofstream file(asciiPath, std::ios::binary);

        if (!file)
            return fail("could not create an ASCII PPM fixture");

        file << "P3\n1 1\n255\n10 20 30\n";
    }

    const PPMImage ascii = readPPMFile(asciiPath);

    std::filesystem::remove(asciiPath);

    if (ascii.width != 1 || ascii.height != 1 || ascii.pixels.size() != 1)
        return fail("PPM reader returned the wrong ASCII image shape");
    if (ascii.pixels[0] != 0xFF1E140Au)
        return fail("PPM reader did not scale ASCII components correctly");

    const std::string truncatedPath = makeTemporaryPath("renderer_regressions-ppm-truncated.ppm");

    {
        std::ofstream file(truncatedPath, std::ios::binary);

        if (!file)
            return fail("could not create a truncated PPM fixture");

        file << "P6\n1 1\n255\n";
        const unsigned char data[] = {1, 2};

        file.write(reinterpret_cast<const char *>(data), sizeof(data));
    }

    try
    {
        (void)readPPMFile(truncatedPath);
        std::filesystem::remove(truncatedPath);
        return fail("PPM reader accepted truncated pixel data");
    }
    catch (const std::runtime_error &)
    {
    }

    std::filesystem::remove(truncatedPath);
    return 0;
}

int checkDielectricStack(void)
{
    DielectricStack stack;

    dielectricReset(stack);

    if (!closeTo(dielectricSurroundingIor(stack), 1.0f) || dielectricFind(stack, 7) >= 0)
        return fail("empty dielectric stack has the wrong state");
    if (!dielectricPush(stack, 7, 1.5f) || !closeTo(dielectricSurroundingIor(stack), 1.5f))
        return fail("dielectric stack did not enter the first interface");
    if (dielectricPush(stack, 7, 1.5f))
        return fail("dielectric stack accepted a duplicate interface");
    if (!dielectricPush(stack, 9, 1.2f) || !closeTo(dielectricSurroundingIor(stack), 1.2f))
        return fail("dielectric stack did not enter the nested interface");
    if (!closeTo(dielectricExteriorIor(stack, 1), 1.5f) || !closeTo(dielectricExteriorIor(stack, 0), 1.0f))
        return fail("dielectric stack reported the wrong exterior IOR");

    dielectricRemove(stack, 9);

    if (dielectricFind(stack, 9) >= 0 || !closeTo(dielectricSurroundingIor(stack), 1.5f))
        return fail("dielectric stack did not exit the nested interface");

    dielectricRemove(stack, 7);

    if (!closeTo(dielectricSurroundingIor(stack), 1.0f))
        return fail("dielectric stack did not return to air");

    for (int i = 0; i < DielectricStack::MAX_INTERFACES; ++i)
    {
        if (!dielectricPush(stack, i, 1.5f))
            return fail("dielectric stack rejected a value below its capacity");
    }
    if (dielectricPush(stack, 99, 1.5f))
        return fail("dielectric stack exceeded its capacity");

    for (int i = 0; i < DielectricStack::MAX_INTERFACES; ++i)
        dielectricRemove(stack, i);
    if (!closeTo(dielectricSurroundingIor(stack), 1.0f))
        return fail("dielectric stack was not empty after removing every interface");

    return 0;
}

int checkTemporaryPaths(void)
{
    const std::string first = makeTemporaryPath("frame.ppm");
    const std::string second = makeTemporaryPath("frame.ppm");

    if (first == second)
        return fail("temporary paths are not unique");
    if (std::filesystem::path(first).extension() != ".ppm")
        return fail("temporary path did not preserve the output extension");

    return 0;
}

int checkCameraFocus(void)
{
    Camera camera(Vec3D(0.0f, 0.0f, 5.0f), Vec3D(0.0f), 60.0f);

    if (!camera.isAutoFocus() || !closeTo(camera.getFocusDistance(), 5.0f, 1e-4f))
        return fail("camera did not start in auto-focus mode");

    camera.initialize(960, 540, Vec3D(0.0f, 0.0f, 10.0f), Vec3D(0.0f), 60.0f);
    if (!camera.isAutoFocus() || !closeTo(camera.getFocusDistance(), 10.0f, 1e-4f))
        return fail("auto-focus did not track the look-at distance");

    camera.setFocusDistance(2.0f);
    if (camera.isAutoFocus() || !closeTo(camera.getFocusDistance(), 2.0f, 1e-4f))
        return fail("explicit focus distance was not stored");

    camera.initialize(960, 540, Vec3D(0.0f, 0.0f, 20.0f), Vec3D(0.0f), 60.0f);
    if (camera.isAutoFocus() || !closeTo(camera.getFocusDistance(), 2.0f, 1e-4f))
        return fail("explicit focus distance did not survive re-initialisation");

    camera.setFocusDistance(0.0f);
    if (!camera.isAutoFocus())
        return fail("zero focus distance did not restore auto-focus");

    camera.initialize(960, 540, Vec3D(0.0f, 0.0f, 30.0f), Vec3D(0.0f), 60.0f);
    if (!closeTo(camera.getFocusDistance(), 30.0f, 1e-4f))
        return fail("restored auto-focus did not track the new look-at distance");

    Camera resetCamera(Vec3D(0.0f, 0.0f, 5.0f), Vec3D(0.0f), 60.0f);
    CameraController controller;

    controller.captureInitial(resetCamera);
    resetCamera.setFocusDistance(2.0f);
    resetCamera.initialize(960, 540, Vec3D(0.0f, 0.0f, 8.0f), Vec3D(0.0f), 60.0f);

    if (!controller.handleEvent('r', resetCamera))
        return fail("camera reset did not report a change");
    if (!resetCamera.isAutoFocus() || !closeTo(resetCamera.getFocusDistance(), 5.0f, 1e-4f))
        return fail("camera reset did not restore the initial auto-focus state");

    return 0;
}

int checkAtmosphereSampling(void)
{
    Atmosphere none;

    none.density = 0.0f;
    if (!closeTo(atmosphereTransmittance(none, Vec3D(0.0f, 1.0f, 0.0f), Vec3D(0.0f, 1.0f, 0.0f), 10.0f), 1.0f))
        return fail("inactive atmosphere must not attenuate");

    Atmosphere homogeneous;

    homogeneous.type = AtmosphereType::HOMOGENEOUS;
    homogeneous.density = 0.5f;

    if (!closeTo(atmosphereTransmittance(homogeneous, Vec3D(0.0f), Vec3D(1.0f, 0.0f, 0.0f), 2.0f),
                 std::exp(-1.0f), 1e-4f))
        return fail("homogeneous transmittance is wrong");

    homogeneous.density = 100.0f;
    if (!(atmosphereTransmittance(homogeneous, Vec3D(0.0f), Vec3D(1.0f, 0.0f, 0.0f), 10.0f) < 1e-6f))
        return fail("dense homogeneous atmosphere is not opaque");

    Atmosphere height;

    height.type = AtmosphereType::HEIGHT;
    height.density = 0.5f;
    height.groundY = 0.0f;
    height.heightFalloff = 0.5f;

    const float lowDepth = atmosphereOpticalDepth(height, Vec3D(0.0f, 0.0f, 0.0f), Vec3D(1.0f, 0.0f, 0.0f), 10.0f);
    const float highDepth = atmosphereOpticalDepth(height, Vec3D(0.0f, 5.0f, 0.0f), Vec3D(1.0f, 0.0f, 0.0f), 10.0f);

    if (!(lowDepth > highDepth) || !(highDepth > 0.0f))
        return fail("height fog is not denser near the ground");

    float distance = 0.0f;

    homogeneous.type = AtmosphereType::HOMOGENEOUS;
    homogeneous.density = 0.5f;
    if (!sampleAtmosphereDistance(homogeneous, Vec3D(0.0f), Vec3D(1.0f, 0.0f, 0.0f), 0.5f, distance))
        return fail("homogeneous medium distance sampling failed");
    if (!closeTo(distance, -std::log(0.5f) / 0.5f, 1e-4f))
        return fail("homogeneous medium distance sampling is wrong");

    if (!sampleAtmosphereDistance(height, Vec3D(0.0f, 1.0f, 0.0f), Vec3D(0.0f, 1.0f, 0.0f), 0.25f, distance))
        return fail("height fog distance sampling failed");
    if (!(distance > 0.0f) || !std::isfinite(distance))
        return fail("height fog distance sampling returned an invalid distance");

    // A downward exponential profile can overflow its optical depth. That
    // must become opaque, not transparent.
    Atmosphere steepHeight;

    steepHeight.type = AtmosphereType::HEIGHT;
    steepHeight.density = 1.0f;
    steepHeight.groundY = 0.0f;
    steepHeight.heightFalloff = 2.0f;
    steepHeight.maxDistance = 1000.0f;

    const float steepDepth =
        atmosphereOpticalDepth(steepHeight, Vec3D(0.0f, 10.0f, 0.0f), Vec3D(0.0f, -1.0f, 0.0f), 1000.0f);

    if (!std::isinf(steepDepth) || steepDepth < 0.0f)
        return fail("overflowing height fog must have infinite optical depth");
    if (atmosphereTransmittance(steepHeight, Vec3D(0.0f, 10.0f, 0.0f), Vec3D(0.0f, -1.0f, 0.0f), 1000.0f) != 0.0f)
        return fail("overflowing height fog must be opaque");

    float integral = 0.0f;
    const int steps = 4096;

    for (int i = 0; i < steps; ++i)
    {
        const float cosine = -1.0f + 2.0f * (static_cast<float>(i) + 0.5f) / static_cast<float>(steps);

        integral += 2.0f * static_cast<float>(M_PI) * henyeyGreensteinPhase(cosine, 0.4f) *
                    (2.0f / static_cast<float>(steps));
    }
    if (integral < 0.95f || integral > 1.05f)
        return fail("Henyey-Greenstein phase function does not integrate to 1");

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
        SceneBuilder wrappedSourceBuilder;

        wrappedSourceBuilder.addMaterial(
            factory.create<IMaterial>("DiffuseMaterial", 0.2, 0.8, Color3D(1.0f, 1.0f, 1.0f)));
        wrappedSourceBuilder.addPrimitive(std::make_shared<InstancePrimitive>(
            std::make_shared<NonCloneablePrimitive>(), TransformData{}));
        const std::shared_ptr<Scene> wrappedSource = wrappedSourceBuilder.build();
        SceneBuilder wrappedDestinationBuilder;

        wrappedDestinationBuilder.addMaterial(
            factory.create<IMaterial>("DiffuseMaterial", 0.2, 0.8, Color3D(1.0f, 1.0f, 1.0f)));

        try
        {
            wrappedDestinationBuilder.addScene(wrappedSource);
        }
        catch (const std::runtime_error &)
        {
            return 0;
        }
        return fail("instance composition accepted a child without clone()");
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

    const std::shared_ptr<const Texture> linear = loadPPMTexture(path, TextureColorSpace::LINEAR);

    if (!linear || !closeTo(linear->getPixels()[0].getX(), encoded, 1e-4f))
        return fail("linear PPM texels were sRGB-decoded");

    const char *invalidPath = "renderer_regressions_invalid.ppm";
    std::ofstream invalid(invalidPath, std::ios::out | std::ios::binary);

    if (!invalid)
        return fail("could not create the invalid PPM");

    invalid << "P6\n1 1\n100\n";
    invalid.put(static_cast<char>(200));
    invalid.put(static_cast<char>(0));
    invalid.put(static_cast<char>(0));
    invalid.close();

    try
    {
        (void)loadPPMTexture(invalidPath);
        return fail("PPM accepted a component above its maximum value");
    }
    catch (const std::runtime_error &)
    {
    }

    return 0;
}

int checkCameraAnimation(void)
{
    const std::vector<CameraKeyframe> keyframes = {
        {0.0, Vec3D(0.0f, 0.0f, -10.0f), Vec3D(0.0f, 0.0f, 0.0f), 60.0f, 0.0f, 0.0f},
        {2.0, Vec3D(10.0f, 0.0f, -10.0f), Vec3D(0.0f, 0.0f, 0.0f), 80.0f, 0.04f, 20.0f}};
    const CameraAnimation animation(keyframes, 24);

    if (animation.getFps() != 24 || !closeTo(static_cast<float>(animation.getDuration()), 2.0f) ||
        animation.getFrameCount() != 48 || animation.getFrameCount(30.0) != 60)
        return fail("animation frame count or duration is wrong");

    const std::shared_ptr<Camera> start = animation.cameraAt(0.0);

    if (!closeTo(start->getLookFrom().getZ(), -10.0f) || !closeTo(start->getVerticalFovDegree(), 60.0f))
        return fail("animation did not reproduce the first keyframe");

    const std::shared_ptr<Camera> middle = animation.cameraAt(1.0);

    if (!closeTo(middle->getLookFrom().getX(), 5.0f, 1e-3f) ||
        !closeTo(middle->getLookFrom().getZ(), -10.0f, 1e-3f) ||
        !closeTo(middle->getVerticalFovDegree(), 70.0f, 1e-3f) ||
        !closeTo(middle->getLensRadius(), 0.02f, 1e-4f) ||
        !closeTo(middle->getFocusDistance(), 10.0f, 1e-3f))
        return fail("animation midpoint interpolation is wrong");

    const std::shared_ptr<Camera> end = animation.cameraAt(2.0);

    if (!closeTo(end->getLookFrom().getX(), 10.0f) || !closeTo(end->getLookFrom().getZ(), -10.0f) ||
        !closeTo(end->getVerticalFovDegree(), 80.0f))
        return fail("animation did not reproduce the last keyframe");

    if (!closeTo(static_cast<float>(animation.timeAtFrame(24, 24.0)), 1.0f) ||
        !closeTo(static_cast<float>(animation.timeAtFrame(1000, 24.0)), 2.0f))
        return fail("animation frame time mapping is wrong");

    try
    {
        animation.cameraAt(std::numeric_limits<double>::quiet_NaN());
        return fail("animation accepted a non-finite time");
    }
    catch (const std::invalid_argument &)
    {
    }

    try
    {
        animation.getFrameCount(std::numeric_limits<double>::max());
        return fail("animation accepted an overflowing frame count");
    }
    catch (const std::invalid_argument &)
    {
    }

    const ShutterSettings shutter{180.0f, 4};

    if (!closeTo(static_cast<float>(shutterSampleTime(1.0, 24.0, shutter, 0)), 0.9921875f, 1e-6f) ||
        !closeTo(static_cast<float>(shutterSampleTime(1.0, 24.0, shutter, 1)), 0.9973958f, 1e-6f) ||
        !closeTo(static_cast<float>(shutterSampleTime(1.0, 24.0, shutter, 2)), 1.0026042f, 1e-6f) ||
        !closeTo(static_cast<float>(shutterSampleTime(1.0, 24.0, shutter, 3)), 1.0078125f, 1e-6f))
        return fail("shutter sample times are wrong");

    if (!closeTo(static_cast<float>(shutterSampleTime(1.0, 24.0, ShutterSettings{0.0f, 4}, 0)), 1.0f) ||
        !closeTo(static_cast<float>(shutterSampleTime(1.0, 24.0, ShutterSettings{180.0f, 1}, 0)), 1.0f))
        return fail("disabled shutter did not preserve the frame time");

    auto expectThrow = [](const std::vector<CameraKeyframe> &keys, int fps, const std::string &label) {
        try
        {
            const CameraAnimation invalid(keys, fps);
            (void)invalid;
        }
        catch (const std::invalid_argument &)
        {
            return true;
        }
        std::cerr << "renderer_regressions: animation accepted " << label << std::endl;
        return false;
    };

    if (!expectThrow({{0.0, Vec3D(0.0f, 0.0f, -5.0f), Vec3D(0.0f, 0.0f, 0.0f), 60.0f, 0.0f, 0.0f}}, 24,
                     "a single keyframe"))
        return 1;
    if (!expectThrow({{1.0, Vec3D(0.0f, 0.0f, -5.0f), Vec3D(0.0f, 0.0f, 0.0f), 60.0f, 0.0f, 0.0f},
                      {2.0, Vec3D(1.0f, 0.0f, -5.0f), Vec3D(0.0f, 0.0f, 0.0f), 60.0f, 0.0f, 0.0f}},
                     24, "a first keyframe after time zero"))
        return 1;
    if (!expectThrow({{0.0, Vec3D(0.0f, 0.0f, -5.0f), Vec3D(0.0f, 0.0f, 0.0f), 60.0f, 0.0f, 0.0f},
                      {2.0, Vec3D(1.0f, 0.0f, -5.0f), Vec3D(0.0f, 0.0f, 0.0f), 60.0f, 0.0f, 0.0f},
                      {2.0, Vec3D(2.0f, 0.0f, -5.0f), Vec3D(0.0f, 0.0f, 0.0f), 60.0f, 0.0f, 0.0f}},
                     24, "duplicate keyframe times"))
        return 1;
    if (!expectThrow({{0.0, Vec3D(0.0f, 0.0f, -5.0f), Vec3D(0.0f, 0.0f, 0.0f), 60.0f, 0.0f, 0.0f},
                      {1.0, Vec3D(1.0f, 0.0f, -5.0f), Vec3D(0.0f, 0.0f, 0.0f), 180.0f, 0.0f, 0.0f}},
                     24, "an out-of-range field of view"))
        return 1;
    if (!expectThrow({{0.0, Vec3D(0.0f, 0.0f, -5.0f), Vec3D(0.0f, 0.0f, 0.0f), 60.0f, 0.0f, 0.0f},
                      {1.0, Vec3D(1.0f, 0.0f, -5.0f), Vec3D(0.0f, 0.0f, 0.0f), 60.0f, 0.0f, 0.0f}},
                     0, "a non-positive FPS"))
        return 1;
    if (!expectThrow({{0.0, Vec3D(0.0f, 0.0f, 0.0f), Vec3D(0.0f, 0.0f, 0.0f), 60.0f, 0.0f, 0.0f},
                      {1.0, Vec3D(1.0f, 0.0f, 0.0f), Vec3D(0.0f, 0.0f, 0.0f), 60.0f, 0.0f, 0.0f}},
                     24, "a degenerate first camera pose"))
        return 1;
    if (!expectThrow({{0.0, Vec3D(std::numeric_limits<float>::quiet_NaN(), 0.0f, -5.0f),
                       Vec3D(0.0f, 0.0f, 0.0f), 60.0f, 0.0f, 0.0f},
                      {1.0, Vec3D(1.0f, 0.0f, -5.0f), Vec3D(0.0f, 0.0f, 0.0f), 60.0f, 0.0f, 0.0f}},
                     24, "a non-finite camera position"))
        return 1;

    auto expectShutterThrow = [&](ShutterSettings shutter, const std::string &label) {
        try
        {
            const CameraAnimation invalid(keyframes, 24, shutter);
            (void)invalid;
        }
        catch (const std::invalid_argument &)
        {
            return true;
        }
        std::cerr << "renderer_regressions: animation accepted " << label << std::endl;
        return false;
    };

    if (!expectShutterThrow(ShutterSettings{360.0f, 4}, "a full shutter angle") ||
        !expectShutterThrow(ShutterSettings{180.0f, 0}, "zero shutter samples"))
        return 1;
    return 0;
}

int checkFramePathFormatting(void)
{
    if (formatFramePath("frame-%04d.png", 7) != "frame-0007.png" ||
        formatFramePath("out/frame-%d.ppm", 12) != "out/frame-12.ppm" ||
        formatFramePath("frame-%u.ppm", 7) != "frame-7.ppm" ||
        formatFramePath("100%%-%03d.png", 3) != "100%-003.png" ||
        formatFramePath("shot.png", 3) != "shot-0003.png" ||
        formatFramePath("shot", 42) != "shot-0042.ppm" ||
        formatFramePath("", 5) != "frame-0005.ppm")
        return fail("frame path formatting produced an unexpected name");

    auto expectThrow = [](const std::string &pattern) {
        try
        {
            formatFramePath(pattern, 1);
        }
        catch (const std::invalid_argument &)
        {
            return true;
        }
        std::cerr << "renderer_regressions: frame path accepted " << pattern << std::endl;
        return false;
    };

    if (!expectThrow("frame-%s.png") || !expectThrow("frame-%d-%d.png"))
        return 1;
    return 0;
}

int checkQuaternionTransform(void)
{
    const Vec3D euler(20.0f, -35.0f, 50.0f);
    const Vec3D translation(1.0f, 2.0f, 3.0f);
    const Vec3D scale(2.0f, 0.5f, 1.5f);
    const TransformData expected = makeTransform(translation, euler, scale);
    const TransformData actual = makeTransform(translation, quaternionFromEulerDegrees(euler), scale);

    for (int i = 0; i < 9; ++i)
    {
        if (!closeTo(actual.linear[i], expected.linear[i], 1e-4f))
            return fail("quaternion and Euler transforms disagree");
    }

    const Quaternion first = quaternionFromEulerDegrees(Vec3D(0.0f, 350.0f, 0.0f));
    const Quaternion second = quaternionFromEulerDegrees(Vec3D(0.0f, 10.0f, 0.0f));
    const Quaternion midpoint = slerp(first, second, 0.5f);
    const TransformData midpointTransform = makeTransform(Vec3D(0.0f), midpoint, Vec3D(1.0f));
    const TransformData zeroTransform =
        makeTransform(Vec3D(0.0f), Vec3D(0.0f), Vec3D(1.0f));

    for (int i = 0; i < 9; ++i)
    {
        if (!closeTo(midpointTransform.linear[i], zeroTransform.linear[i], 1e-3f))
            return fail("quaternion slerp did not take the shortest path");
    }
    return 0;
}

int checkSceneAnimation(void)
{
    Factory factory;
    SceneBuilder builder;

    builder.addMaterial(factory.create<IMaterial>("DiffuseMaterial", 0.2, 0.8, Color3D(1.0f)));
    const std::shared_ptr<IPrimitive> sphere =
        factory.create<IPrimitive>("Sphere", 1.0, Vec3D(0.0f), 0);
    const std::shared_ptr<InstancePrimitive> instance = std::make_shared<InstancePrimitive>(
        sphere, TransformPose{Vec3D(0.0f), Vec3D(0.0f), Vec3D(1.0f)});

    instance->setGroupId("mover");
    builder.addPrimitive(instance);

    const std::shared_ptr<ILight> light =
        factory.create<ILight>("PointLight", Vec3D(0.0f, 2.0f, 0.0f), Color3D(1.0f), 1.0, 1.0, 0.0, 0.0);
    builder.addLight(light, "pulse");

    const std::shared_ptr<Scene> scene = builder.build();
    scene->addInstanceGroup("mover", {instance});

    InstanceTrack instanceTrack;
    instanceTrack.target = "mover";
    instanceTrack.position.keyframes = {{0.0, Vec3D(0.0f)}, {2.0, Vec3D(2.0f, 0.0f, 0.0f)}};
    instanceTrack.scale.keyframes = {{0.0, Vec3D(100.0f)}, {1.0, Vec3D(1.0f)}, {2.0, Vec3D(1.0f)}};

    LightTrack lightTrack;
    lightTrack.target = "pulse";
    lightTrack.intensity.keyframes = {{0.0, 1.0f}, {2.0, 2.0f}};
    lightTrack.position.keyframes = {{0.0, Vec3D(0.0f, 2.0f, 0.0f)}, {2.0, Vec3D(4.0f, 2.0f, 0.0f)}};

    const SceneAnimation animation({instanceTrack}, {lightTrack}, 2.0);

    if (!animation.apply(*scene, 1.0))
        return fail("scene animation did not report instance changes");

    const Vec3D translated = transformPoint(instance->getTransform(), Vec3D(0.0f));
    if (!closeTo(translated.getX(), 1.0f, 1e-4f))
        return fail("scene animation did not interpolate the instance position");

    LightData data;
    if (!light->getData(data) || !closeTo(data.intensity, 1.5f, 1e-4f) ||
        !closeTo(data.position.getX(), 2.0f, 1e-4f))
        return fail("scene animation did not interpolate the light values");

    if (!animation.apply(*scene, 1.5))
        return fail("scene animation did not report the scale change");
    if (!isInvertibleTransform(instance->getTransform()))
        return fail("scene animation produced a non-invertible scale");
    if (!closeTo(instance->getTransform().linear[0], 1.0f, 1e-4f))
        return fail("scene animation scale overshot its keyframes");
    if (animation.apply(*scene, 1.5))
        return fail("scene animation reported an unchanged transform as changed");

    try
    {
        const SceneAnimation invalid({}, {}, 0.0);
        (void)invalid;
        return fail("scene animation accepted a zero duration");
    }
    catch (const std::invalid_argument &)
    {
    }
    return 0;
}

int checkPrefabLightAnimation(void)
{
    Factory factory;
    SceneBuilder prefabBuilder;

    prefabBuilder.addMaterial(factory.create<IMaterial>("DiffuseMaterial", 0.2, 0.8, Color3D(1.0f)));
    prefabBuilder.addPrimitive(factory.create<IPrimitive>("Sphere", 1.0, Vec3D(0.0f), 0));
    prefabBuilder.addLight(
        factory.create<ILight>("PointLight", Vec3D(0.0f, 1.0f, 0.0f), Color3D(1.0f), 1.0, 1.0, 0.0, 0.0));

    const std::shared_ptr<Scene> prefab = prefabBuilder.build();
    SceneBuilder builder;

    builder.addScene(prefab, TransformPose{Vec3D(0.0f), Vec3D(0.0f), Vec3D(1.0f)}, "orbiter");

    const std::shared_ptr<Scene> scene = builder.build();
    InstanceTrack track;

    track.target = "orbiter";
    track.position.keyframes = {{0.0, Vec3D(0.0f)}, {1.0, Vec3D(5.0f, 0.0f, 0.0f)}};

    const SceneAnimation animation({track}, {}, 1.0);

    if (!animation.apply(*scene, 1.0))
        return fail("prefab instance animation did not report a change");

    const Lights &lights = scene->getLights();
    LightData data;

    if (lights.size() != 1 || !lights.front()->getData(data))
        return fail("prefab light was not collected");
    if (!closeTo(data.position.getX(), 5.0f, 1e-4f) || !closeTo(data.position.getY(), 1.0f, 1e-4f))
        return fail("prefab light did not follow the animated instance");

    if (animation.apply(*scene, 1.0))
        return fail("prefab instance animation reported an unchanged transform");
    return 0;
}

int checkVideoSettings(void)
{
    if (!isVideoExtension("out.mp4") || !isVideoExtension("out.MOV") || !isVideoExtension("out.webm") ||
        !isVideoExtension("out.mkv") || isVideoExtension("out.png") || isVideoExtension("out.avi"))
        return fail("video extension detection is wrong");

    if (defaultVideoCodec("out.mp4") != "h264" || defaultVideoCodec("out.mkv") != "h264" ||
        defaultVideoCodec("out.webm") != "vp9")
        return fail("video codec defaults are wrong");

    VideoSettings valid{"out.mp4", "h264", 18, "medium", "", 24.0, 640, 360};

    if (!validateVideoSettings(valid).empty())
        return fail("a valid H.264 video configuration was rejected");

    valid.codec = "vp9";
    if (validateVideoSettings(valid).empty())
        return fail("VP9 was accepted in an MP4 container");

    valid = {"out.webm", "vp9", 32, "medium", "", 24.0, 640, 360};
    if (!validateVideoSettings(valid).empty())
        return fail("a valid VP9 video configuration was rejected");

    valid.codec = "h264";
    if (validateVideoSettings(valid).empty())
        return fail("H.264 was accepted in a WebM container");

    valid = {"out.mp4", "h264", 52, "medium", "", 24.0, 640, 360};
    if (validateVideoSettings(valid).empty())
        return fail("an out-of-range H.264 CRF was accepted");

    valid = {"out.webm", "vp9", 64, "medium", "", 24.0, 640, 360};
    if (validateVideoSettings(valid).empty())
        return fail("an out-of-range VP9 CRF was accepted");

    valid = {"out.mp4", "h264", 18, "turbo", "", 24.0, 640, 360};
    if (validateVideoSettings(valid).empty())
        return fail("an unknown H.264 preset was accepted");

    valid = {"out.mp4", "h264", 18, "medium", "bananas", 24.0, 640, 360};
    if (validateVideoSettings(valid).empty())
        return fail("an invalid video bitrate was accepted");

    valid = {"out.mp4", "h264", 18, "medium", "", 0.0, 640, 360};
    if (validateVideoSettings(valid).empty())
        return fail("a non-positive video frame rate was accepted");

    valid = {"out.mp4", "h264", 18, "medium", "", 24.0, 0, 360};
    if (validateVideoSettings(valid).empty())
        return fail("an empty video width was accepted");

    return 0;
}
} // namespace

int main(int ac, char **av)
{
    if (ac != 4)
    {
        std::cerr << "usage: " << av[0] << " <mtl-scene> <hdr-environment> <mtl-map-scene>" << std::endl;
        return 2;
    }

    Factory factory;

    if (const int status = checkPhongCoefficients(av[1]); status != 0)
        return status;
    if (const int status = checkMtlMaps(av[3]); status != 0)
        return status;
    if (const int status = checkConcaveMesh(); status != 0)
        return status;
    if (const int status = checkPPMReader(); status != 0)
        return status;
    if (const int status = checkDielectricStack(); status != 0)
        return status;
    if (const int status = checkVectorAccess(); status != 0)
        return status;
    if (const int status = checkTemporaryPaths(); status != 0)
        return status;
    if (const int status = checkCameraFocus(); status != 0)
        return status;
    if (const int status = checkAtmosphereSampling(); status != 0)
        return status;
    if (const int status = checkHDREnvironment(av[2]); status != 0)
        return status;
    if (const int status = checkMaterialOverride(factory); status != 0)
        return status;
    if (const int status = checkCloneRequirement(factory); status != 0)
        return status;
    if (const int status = checkSelfHitEpsilon(factory); status != 0)
        return status;
    if (const int status = checkTextureColorSpace(); status != 0)
        return status;
    if (const int status = checkCameraAnimation(); status != 0)
        return status;
    if (const int status = checkFramePathFormatting(); status != 0)
        return status;
    if (const int status = checkQuaternionTransform(); status != 0)
        return status;
    if (const int status = checkSceneAnimation(); status != 0)
        return status;
    if (const int status = checkPrefabLightAnimation(); status != 0)
        return status;
    if (const int status = checkVideoSettings(); status != 0)
        return status;

    std::cout << "renderer_regressions: all checks passed" << std::endl;
    return 0;
}
