/*
** EPITECH PROJECT, 2024
** raytracer
** File description:
** SceneAnimation
*/

#include "render/SceneAnimation.hpp"
#include "Scene.hpp"
#include "render/Animation.hpp"
#include <algorithm>
#include <cmath>
#include <set>
#include <stdexcept>
#include <utility>

using namespace RayTracer;

namespace
{
double clampTime(double time, double duration)
{
    if (!std::isfinite(time))
        throw std::invalid_argument("Scene animation time must be finite");
    return std::clamp(time, 0.0, duration);
}

template <typename Keyframe>
std::size_t findSegment(const std::vector<Keyframe> &keyframes, double time)
{
    std::size_t segment = 0;

    while (segment + 1 < keyframes.size() && time > keyframes[segment + 1].time)
        ++segment;
    return segment;
}

template <typename Keyframe>
float segmentAmount(const Keyframe &first, const Keyframe &second, double time)
{
    return static_cast<float>((time - first.time) / (second.time - first.time));
}

bool finiteVector(const Vec3D &value)
{
    return std::isfinite(value.getX()) && std::isfinite(value.getY()) && std::isfinite(value.getZ());
}

template <typename Keyframe>
void validateTimes(const std::vector<Keyframe> &keyframes, double duration, const std::string &name)
{
    if (keyframes.empty())
        return;
    if (keyframes.size() < 2)
        throw std::runtime_error(name + " requires at least two keyframes");
    if (std::fabs(keyframes.front().time) > 1e-9)
        throw std::runtime_error(name + " must start at time zero");

    for (std::size_t i = 0; i < keyframes.size(); ++i)
    {
        if (!std::isfinite(keyframes[i].time) || keyframes[i].time < 0.0)
            throw std::runtime_error(name + " keyframe time must be finite and non-negative");
        if (keyframes[i].time > duration + 1e-9)
            throw std::runtime_error(name + " keyframe time exceeds the animation duration");
        if (i > 0 && keyframes[i].time <= keyframes[i - 1].time)
            throw std::runtime_error(name + " keyframe times must be strictly increasing");
    }
}
} // namespace

Vec3D Vec3Track::at(double time) const
{
    if (keyframes.empty())
        return Vec3D(0.0f);

    if (keyframes.size() == 1 || time <= keyframes.front().time)
        return keyframes.front().value;
    if (time >= keyframes.back().time)
        return keyframes.back().value;

    const std::size_t segment = findSegment(keyframes, time);
    const Vec3Keyframe &first = keyframes[segment];
    const Vec3Keyframe &second = keyframes[segment + 1];
    const Vec3Keyframe &before = segment == 0 ? first : keyframes[segment - 1];
    const Vec3Keyframe &after = segment + 2 >= keyframes.size() ? second : keyframes[segment + 2];

    return catmullRom(before.value, first.value, second.value, after.value,
                      segmentAmount(first, second, time));
}

float FloatTrack::at(double time) const
{
    if (keyframes.empty())
        return 0.0f;

    if (keyframes.size() == 1 || time <= keyframes.front().time)
        return keyframes.front().value;
    if (time >= keyframes.back().time)
        return keyframes.back().value;

    const std::size_t segment = findSegment(keyframes, time);
    const FloatKeyframe &first = keyframes[segment];
    const FloatKeyframe &second = keyframes[segment + 1];
    const float amount = segmentAmount(first, second, time);

    return first.value + (second.value - first.value) * amount;
}

Color3D ColorTrack::at(double time) const
{
    if (keyframes.empty())
        return Color3D(0.0f);

    if (keyframes.size() == 1 || time <= keyframes.front().time)
        return keyframes.front().value;
    if (time >= keyframes.back().time)
        return keyframes.back().value;

    const std::size_t segment = findSegment(keyframes, time);
    const ColorKeyframe &first = keyframes[segment];
    const ColorKeyframe &second = keyframes[segment + 1];
    const float amount = segmentAmount(first, second, time);

    return first.value + (second.value - first.value) * amount;
}

Quaternion RotationTrack::at(double time) const
{
    if (keyframes.empty())
        return Quaternion{};

    if (keyframes.size() == 1 || time <= keyframes.front().time)
        return quaternionFromEulerDegrees(keyframes.front().value);
    if (time >= keyframes.back().time)
        return quaternionFromEulerDegrees(keyframes.back().value);

    const std::size_t segment = findSegment(keyframes, time);
    const Vec3Keyframe &first = keyframes[segment];
    const Vec3Keyframe &second = keyframes[segment + 1];

    return slerp(quaternionFromEulerDegrees(first.value), quaternionFromEulerDegrees(second.value),
                 segmentAmount(first, second, time));
}

SceneAnimation::SceneAnimation(std::vector<InstanceTrack> instances, std::vector<LightTrack> lights, double duration)
    : _instances(std::move(instances)), _lights(std::move(lights)), _duration(duration)
{
    if (!std::isfinite(_duration) || _duration <= 0.0)
        throw std::invalid_argument("Scene animation duration must be positive");

    std::set<std::string> instanceTargets;
    std::set<std::string> lightTargets;

    for (const InstanceTrack &track : _instances)
    {
        if (track.target.empty())
            throw std::runtime_error("Instance animation target must not be empty");
        if (track.empty())
            throw std::runtime_error("Instance animation track '" + track.target + "' has no channels");
        if (!instanceTargets.insert(track.target).second)
            throw std::runtime_error("Duplicate instance animation target: " + track.target);

        validateTimes(track.position.keyframes, _duration, "Instance position track");
        validateTimes(track.rotation.keyframes, _duration, "Instance rotation track");
        validateTimes(track.scale.keyframes, _duration, "Instance scale track");

        for (const Vec3Keyframe &keyframe : track.position.keyframes)
            if (!finiteVector(keyframe.value))
                throw std::runtime_error("Instance position keyframe contains a non-finite value");
        for (const Vec3Keyframe &keyframe : track.rotation.keyframes)
            if (!finiteVector(keyframe.value))
                throw std::runtime_error("Instance rotation keyframe contains a non-finite value");
        for (const Vec3Keyframe &keyframe : track.scale.keyframes)
        {
            if (!finiteVector(keyframe.value) || keyframe.value.getX() == 0.0f || keyframe.value.getY() == 0.0f ||
                keyframe.value.getZ() == 0.0f)
                throw std::runtime_error("Instance scale keyframes must be finite and non-zero");
        }
    }

    for (const LightTrack &track : _lights)
    {
        if (track.target.empty())
            throw std::runtime_error("Light animation target must not be empty");
        if (track.empty())
            throw std::runtime_error("Light animation track '" + track.target + "' has no channels");
        if (!lightTargets.insert(track.target).second)
            throw std::runtime_error("Duplicate light animation target: " + track.target);

        validateTimes(track.intensity.keyframes, _duration, "Light intensity track");
        validateTimes(track.color.keyframes, _duration, "Light color track");
        validateTimes(track.position.keyframes, _duration, "Light position track");

        for (const FloatKeyframe &keyframe : track.intensity.keyframes)
            if (!std::isfinite(keyframe.value) || keyframe.value < 0.0f)
                throw std::runtime_error("Light intensity keyframes must be finite and non-negative");
        for (const ColorKeyframe &keyframe : track.color.keyframes)
            if (!finiteVector(keyframe.value) || keyframe.value.getX() < 0.0f || keyframe.value.getY() < 0.0f ||
                keyframe.value.getZ() < 0.0f)
                throw std::runtime_error("Light color keyframes must be finite and non-negative");
        for (const Vec3Keyframe &keyframe : track.position.keyframes)
            if (!finiteVector(keyframe.value))
                throw std::runtime_error("Light position keyframe contains a non-finite value");
    }
}

bool SceneAnimation::apply(Scene &scene, double time) const
{
    const double clampedTime = clampTime(time, _duration);
    bool instancesChanged = false;

    for (const InstanceTrack &track : _instances)
    {
        const InstanceGroup *group = scene.findInstanceGroup(track.target);

        if (!group || group->empty())
            throw std::runtime_error("Unknown animated instance: " + track.target);
        if (!group->front()->hasPose())
            throw std::runtime_error("Animated instance has no base transform: " + track.target);

        const TransformPose &base = group->front()->getPose();
        const Vec3D translation = track.position.empty() ? base.translation : track.position.at(clampedTime);
        const Vec3D scale = track.scale.empty() ? base.scale : track.scale.at(clampedTime);
        TransformData transform;

        if (track.rotation.empty())
            transform = makeTransform(translation, base.rotation, scale);
        else if (clampedTime <= track.rotation.keyframes.front().time)
            transform = makeTransform(translation, track.rotation.keyframes.front().value, scale);
        else if (clampedTime >= track.rotation.keyframes.back().time)
            transform = makeTransform(translation, track.rotation.keyframes.back().value, scale);
        else
            transform = makeTransform(translation, track.rotation.at(clampedTime), scale);

        for (const std::shared_ptr<InstancePrimitive> &instance : *group)
            instance->setTransform(transform);
        instancesChanged = true;
    }

    for (const LightTrack &track : _lights)
    {
        const std::shared_ptr<ILight> light = scene.findLight(track.target);

        if (!light)
            throw std::runtime_error("Unknown animated light: " + track.target);

        if (!track.intensity.empty())
            light->setIntensity(track.intensity.at(clampedTime));
        if (!track.color.empty())
            light->setColor(track.color.at(clampedTime));
        if (!track.position.empty())
            light->setPosition(track.position.at(clampedTime));
    }

    return instancesChanged;
}

bool SceneAnimation::animatesInstances(void) const
{
    return !_instances.empty();
}

bool SceneAnimation::animatesLights(void) const
{
    return !_lights.empty();
}

double SceneAnimation::getDuration(void) const
{
    return _duration;
}

const std::vector<InstanceTrack> &SceneAnimation::getInstanceTracks(void) const
{
    return _instances;
}

const std::vector<LightTrack> &SceneAnimation::getLightTracks(void) const
{
    return _lights;
}
