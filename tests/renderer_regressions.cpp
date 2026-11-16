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
#include "render/Animation.hpp"
#include "render/Environment.hpp"
#include "render/ImageWriter.hpp"
#include "render/SceneAnimation.hpp"
#include "render/Texture.hpp"
#include "render/VideoWriter.hpp"
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

    const std::shared_ptr<const Texture> linear = loadPPMTexture(path, TextureColorSpace::LINEAR);

    if (!linear || !closeTo(linear->getPixels()[0].getX(), encoded, 1e-4f))
        return fail("linear PPM texels were sRGB-decoded");

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
    if (const int status = checkCameraAnimation(); status != 0)
        return status;
    if (const int status = checkFramePathFormatting(); status != 0)
        return status;
    if (const int status = checkQuaternionTransform(); status != 0)
        return status;
    if (const int status = checkSceneAnimation(); status != 0)
        return status;
    if (const int status = checkVideoSettings(); status != 0)
        return status;

    std::cout << "renderer_regressions: all checks passed" << std::endl;
    return 0;
}
