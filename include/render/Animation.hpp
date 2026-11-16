/*
** EPITECH PROJECT, 2024
** raytracer
** File description:
** Animation
*/

#ifndef ANIMATION_HPP_
#define ANIMATION_HPP_

#include "Camera.hpp"
#include "Vec3D.hpp"
#include <memory>
#include <vector>

namespace RayTracer
{

struct CameraKeyframe
{
    double time = 0.0;
    Vec3D position;
    Vec3D lookAt;
    float fieldOfView = 60.0f;
    float lensRadius = 0.0f;
    float focusDistance = 0.0f;
};

struct ShutterSettings
{
    float angle = 0.0f;
    int samples = 1;
};

/**
 * Camera-only animation timeline. Position and look-at use Catmull-Rom
 * interpolation; FOV, lens radius and focus distance are interpolated
 * linearly. Times are in seconds and the first keyframe must be at t=0.
 */
class CameraAnimation
{
  public:
    CameraAnimation(std::vector<CameraKeyframe> keyframes, int fps, ShutterSettings shutter = {});

    std::shared_ptr<Camera> cameraAt(double time) const;
    double getDuration(void) const;
    int getFps(void) const;
    int getFrameCount(void) const;
    int getFrameCount(double fps) const;
    double timeAtFrame(int frame, double fps) const;
    const std::vector<CameraKeyframe> &getKeyframes(void) const;
    const ShutterSettings &getShutter(void) const;

  private:
    std::vector<CameraKeyframe> _keyframes;
    int _fps;
    ShutterSettings _shutter;
};

/**
 * Returns the deterministic time of one shutter sample for a frame. The
 * exposure interval is centered on the frame time and the sample offsets are
 * stratified so CPU and CUDA use exactly the same subframe times.
 */
double shutterSampleTime(double frameTime, double fps, const ShutterSettings &shutter, int sample);

} // namespace RayTracer

#endif // ANIMATION_HPP_
