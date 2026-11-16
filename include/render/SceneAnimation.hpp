/*
** EPITECH PROJECT, 2024
** raytracer
** File description:
** SceneAnimation
*/

#ifndef SCENE_ANIMATION_HPP_
#define SCENE_ANIMATION_HPP_

#include "Color.hpp"
#include "Transform.hpp"
#include "Vec3D.hpp"
#include <string>
#include <vector>

namespace RayTracer
{

class Scene;

struct Vec3Keyframe
{
    double time = 0.0;
    Vec3D value;
};

struct FloatKeyframe
{
    double time = 0.0;
    float value = 0.0f;
};

struct ColorKeyframe
{
    double time = 0.0;
    Color3D value;
};

struct Vec3Track
{
    std::vector<Vec3Keyframe> keyframes;

    bool empty(void) const
    {
        return keyframes.empty();
    }

    Vec3D at(double time) const;

    // Scale tracks use linear interpolation to avoid Catmull-Rom overshoot
    // through zero or into negative values.
    Vec3D atLinear(double time) const;
};

struct FloatTrack
{
    std::vector<FloatKeyframe> keyframes;

    bool empty(void) const
    {
        return keyframes.empty();
    }

    float at(double time) const;
};

struct ColorTrack
{
    std::vector<ColorKeyframe> keyframes;

    bool empty(void) const
    {
        return keyframes.empty();
    }

    Color3D at(double time) const;
};

struct RotationTrack
{
    std::vector<Vec3Keyframe> keyframes;

    bool empty(void) const
    {
        return keyframes.empty();
    }

    Quaternion at(double time) const;
};

struct InstanceTrack
{
    std::string target;
    Vec3Track position;
    RotationTrack rotation;
    Vec3Track scale;

    bool empty(void) const
    {
        return position.empty() && rotation.empty() && scale.empty();
    }
};

struct LightTrack
{
    std::string target;
    FloatTrack intensity;
    ColorTrack color;
    Vec3Track position;

    bool empty(void) const
    {
        return intensity.empty() && color.empty() && position.empty();
    }
};

/**
 * Animated prefab-instance transforms and light values. The camera timeline
 * remains a separate CameraAnimation. Tracks without a channel inherit the
 * instance's base transform or leave the light value unchanged.
 */
class SceneAnimation
{
  public:
    SceneAnimation(std::vector<InstanceTrack> instances, std::vector<LightTrack> lights, double duration);

    /** Applies the timeline at the given time; returns true if instances moved. */
    bool apply(Scene &scene, double time) const;

    bool animatesInstances(void) const;
    bool animatesLights(void) const;
    double getDuration(void) const;
    const std::vector<InstanceTrack> &getInstanceTracks(void) const;
    const std::vector<LightTrack> &getLightTracks(void) const;

  private:
    std::vector<InstanceTrack> _instances;
    std::vector<LightTrack> _lights;
    double _duration;
};

} // namespace RayTracer

#endif // SCENE_ANIMATION_HPP_
