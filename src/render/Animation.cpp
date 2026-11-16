/*
** EPITECH PROJECT, 2024
** raytracer
** File description:
** Animation
*/

#include "render/Animation.hpp"
#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>
#include <utility>

using namespace RayTracer;

namespace
{
float lerp(float first, float second, float amount)
{
    return first + (second - first) * amount;
}
} // namespace

Vec3D RayTracer::catmullRom(const Vec3D &p0, const Vec3D &p1, const Vec3D &p2, const Vec3D &p3, float t)
{
    const float t2 = t * t;
    const float t3 = t2 * t;

    return Vec3D(
        0.5f * (2.0f * p1.getX() + (-p0.getX() + p2.getX()) * t +
                (2.0f * p0.getX() - 5.0f * p1.getX() + 4.0f * p2.getX() - p3.getX()) * t2 +
                (-p0.getX() + 3.0f * p1.getX() - 3.0f * p2.getX() + p3.getX()) * t3),
        0.5f * (2.0f * p1.getY() + (-p0.getY() + p2.getY()) * t +
                (2.0f * p0.getY() - 5.0f * p1.getY() + 4.0f * p2.getY() - p3.getY()) * t2 +
                (-p0.getY() + 3.0f * p1.getY() - 3.0f * p2.getY() + p3.getY()) * t3),
        0.5f * (2.0f * p1.getZ() + (-p0.getZ() + p2.getZ()) * t +
                (2.0f * p0.getZ() - 5.0f * p1.getZ() + 4.0f * p2.getZ() - p3.getZ()) * t2 +
                (-p0.getZ() + 3.0f * p1.getZ() - 3.0f * p2.getZ() + p3.getZ()) * t3));
}

CameraAnimation::CameraAnimation(std::vector<CameraKeyframe> keyframes, int fps, ShutterSettings shutter)
    : _keyframes(std::move(keyframes)), _fps(fps), _shutter(shutter)
{
    if (_fps <= 0)
        throw std::invalid_argument("Animation FPS must be positive");
    if (!std::isfinite(_shutter.angle) || _shutter.angle < 0.0f || _shutter.angle >= 360.0f)
        throw std::invalid_argument("Animation shutter angle must be in [0, 360) degrees");
    if (_shutter.samples <= 0)
        throw std::invalid_argument("Animation shutter sample count must be positive");
    if (_keyframes.size() < 2)
        throw std::invalid_argument("Animation requires at least two camera keyframes");
    if (std::fabs(_keyframes.front().time) > 1e-9)
        throw std::invalid_argument("Animation must start at time zero");

    for (std::size_t i = 0; i < _keyframes.size(); ++i)
    {
        const CameraKeyframe &keyframe = _keyframes[i];

        if (!std::isfinite(keyframe.time) || keyframe.time < 0.0 || !std::isfinite(keyframe.fieldOfView) ||
            !std::isfinite(keyframe.lensRadius) || !std::isfinite(keyframe.focusDistance) ||
            !std::isfinite(keyframe.position.getX()) || !std::isfinite(keyframe.position.getY()) ||
            !std::isfinite(keyframe.position.getZ()) || !std::isfinite(keyframe.lookAt.getX()) ||
            !std::isfinite(keyframe.lookAt.getY()) || !std::isfinite(keyframe.lookAt.getZ()))
            throw std::invalid_argument("Animation keyframe contains a non-finite value");
        if (keyframe.fieldOfView <= 0.0f || keyframe.fieldOfView >= 180.0f)
            throw std::invalid_argument("Animation keyframe field of view must be between 0 and 180 degrees");
        if (keyframe.lensRadius < 0.0f)
            throw std::invalid_argument("Animation keyframe lens radius must not be negative");
        if (keyframe.focusDistance < 0.0f)
            throw std::invalid_argument("Animation keyframe focus distance must not be negative");
        if ((keyframe.position - keyframe.lookAt).lengthSquared() < 1e-12f)
            throw std::invalid_argument("Animation keyframe camera position and look-at must differ");

        if (i > 0 && keyframe.time <= _keyframes[i - 1].time)
            throw std::invalid_argument("Animation keyframe times must be strictly increasing");
    }

    if (_keyframes.back().time <= 0.0)
        throw std::invalid_argument("Animation duration must be positive");
}

std::shared_ptr<Camera> CameraAnimation::cameraAt(double time) const
{
    if (!std::isfinite(time))
        throw std::invalid_argument("Animation time must be finite");

    const double clampedTime = std::clamp(time, _keyframes.front().time, _keyframes.back().time);
    std::size_t segment = 0;

    while (segment + 1 < _keyframes.size() && clampedTime > _keyframes[segment + 1].time)
        ++segment;

    const CameraKeyframe &first = _keyframes[segment];
    const CameraKeyframe &second = _keyframes[segment + 1];
    const double interval = second.time - first.time;

    if (interval <= 0.0)
        throw std::runtime_error("Animation keyframe interval is invalid");

    const float amount = static_cast<float>((clampedTime - first.time) / interval);
    const CameraKeyframe &before = segment == 0 ? first : _keyframes[segment - 1];
    const CameraKeyframe &after = segment + 2 >= _keyframes.size() ? second : _keyframes[segment + 2];

    Camera camera(catmullRom(before.lookAt, first.lookAt, second.lookAt, after.lookAt, amount),
                  catmullRom(before.position, first.position, second.position, after.position, amount),
                  lerp(first.fieldOfView, second.fieldOfView, amount));

    const float lensRadius = lerp(first.lensRadius, second.lensRadius, amount);

    if (lensRadius >= 0.0f)
        camera.setLensRadius(lensRadius);

    const float focusDistance = lerp(first.focusDistance, second.focusDistance, amount);

    if (focusDistance > 0.0f)
        camera.setFocusDistance(focusDistance);
    return std::make_shared<Camera>(camera);
}

double CameraAnimation::getDuration(void) const
{
    return _keyframes.back().time;
}

int CameraAnimation::getFps(void) const
{
    return _fps;
}

int CameraAnimation::getFrameCount(void) const
{
    return getFrameCount(static_cast<double>(_fps));
}

int CameraAnimation::getFrameCount(double fps) const
{
    if (!std::isfinite(fps) || fps <= 0.0)
        throw std::invalid_argument("Animation FPS must be positive");

    const double frames = std::ceil(getDuration() * fps);

    if (!std::isfinite(frames) || frames > static_cast<double>(std::numeric_limits<int>::max()))
        throw std::invalid_argument("Animation frame count is too large");
    return std::max(1, static_cast<int>(frames));
}

double CameraAnimation::timeAtFrame(int frame, double fps) const
{
    if (frame < 0 || !std::isfinite(fps) || fps <= 0.0)
        throw std::invalid_argument("Invalid animation frame or FPS");

    return std::min(getDuration(), static_cast<double>(frame) / fps);
}

const std::vector<CameraKeyframe> &CameraAnimation::getKeyframes(void) const
{
    return _keyframes;
}

const ShutterSettings &CameraAnimation::getShutter(void) const
{
    return _shutter;
}

double RayTracer::shutterSampleTime(double frameTime, double fps, const ShutterSettings &shutter, int sample)
{
    if (shutter.angle <= 0.0f || shutter.samples <= 1)
        return frameTime;

    const double exposure = (static_cast<double>(shutter.angle) / 360.0) / fps;
    const double offset = (static_cast<double>(sample) + 0.5) / static_cast<double>(shutter.samples);

    return frameTime - exposure * 0.5 + offset * exposure;
}
