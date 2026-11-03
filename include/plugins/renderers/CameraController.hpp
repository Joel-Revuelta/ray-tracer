/*
** EPITECH PROJECT, 2024
** raytracer
** File description:
** CameraController
*/

#ifndef CAMERA_CONTROLLER_HPP_
#define CAMERA_CONTROLLER_HPP_

#include "Vec3D.hpp"

namespace RayTracer
{

class Camera;

/**
 * Shared interactive camera controls used by renderers with a GUI.
 *
 * Movement: W/A/S/D (camera-relative), Q/E (world up/down).
 * Look: arrow keys or I/J/K/L.
 * Zoom: + / -.
 * R: restore the initial pose. X: lock the look-at point. C: lock the eye.
 */
class CameraController
{
  public:
    // Stores the pose that R restores. Call once before interactive rendering.
    void captureInitial(const Camera &camera);

    // True for keys that can change the camera pose. Locks and unknown keys do
    // not require restarting an in-flight frame.
    bool isCameraEvent(char event) const;

    // Returns true when the camera changed and the render should restart.
    bool handleEvent(char event, Camera &camera);

  private:
    bool applyMovement(char event, Camera &camera);
    bool applyLook(char event, Camera &camera);
    bool applyFov(Camera &camera, float deltaDegrees);
    bool updateCamera(Camera &camera, const Vec3D &lookFrom, const Vec3D &lookAt, float fov);

    bool _lockLookAt = false;
    bool _lockLookFrom = false;
    Vec3D _initialLookFrom;
    Vec3D _initialLookAt;
    float _initialFov = 90.0f;
    bool _hasInitialState = false;
};

} // namespace RayTracer

#endif // CAMERA_CONTROLLER_HPP_
