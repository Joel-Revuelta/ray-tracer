/*
** EPITECH PROJECT, 2023
** raytracer
** File description:
** ParserAnimation.cpp
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
 * Parses the optional camera animation. Keyframes inherit omitted fields from
 * the previous keyframe, starting with the top-level camera.
 */
std::shared_ptr<CameraAnimation> Parser::parseAnimation(const libconfig::Setting &animationSetting)
{
    rejectUnknownKeys(animationSetting, "Animation", {"fps", "shutter", "camera", "instances", "lights"});

    double fpsValue = 24.0;

    if (animationSetting.exists("fps"))
    {
        fpsValue = settingAsDouble(animationSetting.lookup("fps"), "Animation fps");
        requireFinite(fpsValue, "Animation fps");
        if (fpsValue <= 0.0 || fpsValue != std::floor(fpsValue) ||
            fpsValue > static_cast<double>(std::numeric_limits<int>::max()))
            throw std::runtime_error("Animation fps must be a positive integer");
    }

    const int fps = static_cast<int>(fpsValue);
    ShutterSettings shutter;

    if (animationSetting.exists("shutter"))
    {
        const libconfig::Setting &shutterSetting = animationSetting.lookup("shutter");

        rejectUnknownKeys(shutterSetting, "Animation shutter", {"angle", "samples"});

        if (shutterSetting.exists("angle"))
        {
            const double angle = settingAsDouble(shutterSetting.lookup("angle"), "Animation shutter angle");

            requireFinite(angle, "Animation shutter angle");
            if (angle < 0.0 || angle >= 360.0)
                throw std::runtime_error("Animation shutter angle must be in [0, 360) degrees");
            shutter.angle = static_cast<float>(angle);
        }

        if (shutterSetting.exists("samples"))
        {
            shutter.samples = shutterSetting.lookup("samples");
            if (shutter.samples <= 0)
                throw std::runtime_error("Animation shutter sample count must be positive");
        }
    }

    const libconfig::Setting &keyframesSetting = animationSetting.lookup("camera");

    if (!keyframesSetting.isList() || keyframesSetting.getLength() < 2)
        throw std::runtime_error("Animation requires at least two camera keyframes");
    if (static_cast<std::size_t>(keyframesSetting.getLength()) > MAX_ANIMATION_KEYFRAMES)
        throw std::runtime_error("Animation has too many camera keyframes");

    std::vector<CameraKeyframe> keyframes;
    keyframes.reserve(static_cast<std::size_t>(keyframesSetting.getLength()));

    Point3D position = _camera ? _camera->getLookFrom() : Point3D(0.0f, 0.0f, 0.0f);
    Point3D lookAt = _camera ? _camera->getLookAt() : Point3D(0.0f, 0.0f, 1.0f);
    float fieldOfView = _camera ? _camera->getVerticalFovDegree() : 60.0f;
    float lensRadius = _camera ? _camera->getLensRadius() : 0.0f;
    float focusDistance = _camera && !_camera->isAutoFocus() ? _camera->getFocusDistance() : 0.0f;
    double previousTime = -1.0;

    for (int i = 0; i < keyframesSetting.getLength(); ++i)
    {
        const libconfig::Setting &keyframeSetting = keyframesSetting[i];

        rejectUnknownKeys(keyframeSetting, "Camera keyframe",
                          {"time", "position", "rotation", "fieldOfView", "lensRadius", "focusDistance"});

        const double time = settingAsDouble(keyframeSetting.lookup("time"), "Animation keyframe time");

        requireFinite(time, "Animation keyframe time");
        if (time < 0.0)
            throw std::runtime_error("Animation keyframe time must not be negative");
        if (i == 0 && std::fabs(time) > 1e-9)
            throw std::runtime_error("Animation must start at time zero");
        if (time <= previousTime)
            throw std::runtime_error("Animation keyframe times must be strictly increasing");

        if (keyframeSetting.exists("position"))
            position = parsePoint(keyframeSetting.lookup("position"));
        if (keyframeSetting.exists("rotation"))
            lookAt = parsePoint(keyframeSetting.lookup("rotation"));
        if (keyframeSetting.exists("fieldOfView"))
        {
            const double value = settingAsDouble(keyframeSetting.lookup("fieldOfView"), "Animation field of view");

            requireFinite(value, "Animation field of view");
            if (value <= 0.0 || value >= 180.0)
                throw std::runtime_error("Animation field of view must be strictly between 0 and 180 degrees");
            fieldOfView = static_cast<float>(value);
        }
        if (keyframeSetting.exists("lensRadius"))
        {
            const double value = settingAsDouble(keyframeSetting.lookup("lensRadius"), "Animation lens radius");

            requireNonNegative(value, "Animation lens radius");
            lensRadius = static_cast<float>(value);
        }
        if (keyframeSetting.exists("focusDistance"))
        {
            const double value = settingAsDouble(keyframeSetting.lookup("focusDistance"), "Animation focus distance");

            requirePositive(value, "Animation focus distance");
            focusDistance = static_cast<float>(value);
        }

        keyframes.push_back(
            CameraKeyframe{time, position, lookAt, fieldOfView, lensRadius, focusDistance});
        previousTime = time;
    }

    return std::make_shared<CameraAnimation>(std::move(keyframes), fps, shutter);
}

/**
 * Parses animated prefab-instance transforms and light values. The scene must
 * already be built so targets can be validated against the registered ids.
 */
std::shared_ptr<SceneAnimation> Parser::parseSceneAnimation(const libconfig::Setting &animationSetting,
                                                             const Scene &scene)
{
    if (!_animation)
        throw std::runtime_error("Scene animation requires a camera animation timeline");

    rejectUnknownKeys(animationSetting, "Scene animation", {"fps", "shutter", "camera", "instances", "lights"});

    auto forEach = [](const libconfig::Setting &setting, auto callback) {
        if (setting.isList())
        {
            for (int i = 0; i < setting.getLength(); ++i)
                callback(setting[i]);
        }
        else
            callback(setting);
    };

    auto parseVec3Track = [&](const libconfig::Setting &trackSetting, const std::string &name) {
        Vec3Track track;

        forEach(trackSetting, [&](const libconfig::Setting &keyframe) {
            rejectUnknownKeys(keyframe, name + " keyframe", {"time", "value"});

            const double time = settingAsDouble(keyframe.lookup("time"), name + " keyframe time");

            requireFinite(time, name + " keyframe time");
            if (track.keyframes.size() >= MAX_ANIMATION_KEYFRAMES)
                throw std::runtime_error(name + " has too many keyframes");
            track.keyframes.push_back(Vec3Keyframe{time, parsePoint(keyframe.lookup("value"))});
        });
        return track;
    };

    auto parseRotationTrack = [&](const libconfig::Setting &trackSetting, const std::string &name) {
        RotationTrack track;
        const Vec3Track values = parseVec3Track(trackSetting, name);

        track.keyframes = values.keyframes;
        return track;
    };

    auto parseFloatTrack = [&](const libconfig::Setting &trackSetting, const std::string &name) {
        FloatTrack track;

        forEach(trackSetting, [&](const libconfig::Setting &keyframe) {
            rejectUnknownKeys(keyframe, name + " keyframe", {"time", "value"});

            const double time = settingAsDouble(keyframe.lookup("time"), name + " keyframe time");
            const double value = settingAsDouble(keyframe.lookup("value"), name + " keyframe value");

            requireFinite(time, name + " keyframe time");
            requireFinite(value, name + " keyframe value");
            if (track.keyframes.size() >= MAX_ANIMATION_KEYFRAMES)
                throw std::runtime_error(name + " has too many keyframes");
            track.keyframes.push_back(FloatKeyframe{time, static_cast<float>(value)});
        });
        return track;
    };

    auto parseColorTrack = [&](const libconfig::Setting &trackSetting, const std::string &name) {
        ColorTrack track;

        forEach(trackSetting, [&](const libconfig::Setting &keyframe) {
            rejectUnknownKeys(keyframe, name + " keyframe", {"time", "value"});

            const double time = settingAsDouble(keyframe.lookup("time"), name + " keyframe time");

            requireFinite(time, name + " keyframe time");
            if (track.keyframes.size() >= MAX_ANIMATION_KEYFRAMES)
                throw std::runtime_error(name + " has too many keyframes");
            track.keyframes.push_back(ColorKeyframe{time, parseColor(keyframe.lookup("value"))});
        });
        return track;
    };

    std::vector<InstanceTrack> instances;
    std::vector<LightTrack> lights;

    if (animationSetting.exists("instances"))
    {
        const libconfig::Setting &instanceTracks = animationSetting.lookup("instances");

        forEach(instanceTracks, [&](const libconfig::Setting &trackSetting) {
            InstanceTrack track;

            rejectUnknownKeys(trackSetting, "Instance animation track", {"target", "position", "rotation", "scale"});
            track.target = std::string(trackSetting.lookup("target").c_str());
            if (track.target.empty())
                throw std::runtime_error("Instance animation target must not be empty");
            if (!scene.findInstanceGroup(track.target))
                throw std::runtime_error("Unknown animated instance: " + track.target);

            if (trackSetting.exists("position"))
                track.position = parseVec3Track(trackSetting.lookup("position"), "Instance position");
            if (trackSetting.exists("rotation"))
                track.rotation = parseRotationTrack(trackSetting.lookup("rotation"), "Instance rotation");
            if (trackSetting.exists("scale"))
                track.scale = parseVec3Track(trackSetting.lookup("scale"), "Instance scale");

            if (instances.size() >= MAX_ANIMATION_TRACKS)
                throw std::runtime_error("Too many instance animation tracks");
            instances.push_back(std::move(track));
        });
    }

    if (animationSetting.exists("lights"))
    {
        const libconfig::Setting &lightTracks = animationSetting.lookup("lights");

        forEach(lightTracks, [&](const libconfig::Setting &trackSetting) {
            LightTrack track;

            rejectUnknownKeys(trackSetting, "Light animation track",
                              {"target", "intensity", "color", "position"});
            track.target = std::string(trackSetting.lookup("target").c_str());
            if (track.target.empty())
                throw std::runtime_error("Light animation target must not be empty");

            const std::shared_ptr<ILight> light = scene.findLight(track.target);
            if (!light)
                throw std::runtime_error("Unknown animated light: " + track.target);

            if (trackSetting.exists("intensity"))
                track.intensity = parseFloatTrack(trackSetting.lookup("intensity"), "Light intensity");
            if (trackSetting.exists("color"))
                track.color = parseColorTrack(trackSetting.lookup("color"), "Light color");
            if (trackSetting.exists("position"))
            {
                LightData data;

                if (!light->getData(data) ||
                    (data.type != LightType::POINT && data.type != LightType::AREA_SPHERE))
                    throw std::runtime_error(
                        "Light position animation is only supported for point and sphere lights");
                track.position = parseVec3Track(trackSetting.lookup("position"), "Light position");
            }

            if (lights.size() >= MAX_ANIMATION_TRACKS)
                throw std::runtime_error("Too many light animation tracks");
            lights.push_back(std::move(track));
        });
    }

    if (instances.empty() && lights.empty())
        return nullptr;
    return std::make_shared<SceneAnimation>(std::move(instances), std::move(lights), _animation->getDuration());
}
