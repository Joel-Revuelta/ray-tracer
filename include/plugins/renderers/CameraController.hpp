/*
** EPITECH PROJECT, 2024
** raytracer
** File description:
** CameraController
*/

#ifndef CAMERA_CONTROLLER_HPP_
#define CAMERA_CONTROLLER_HPP_

namespace RayTracer
{

class Camera;

/**
 * Shared WASD/QE/R/X/C camera controls used by renderers with a GUI.
 */
class CameraController
{
  public:
    // Returns true when the camera changed and the render should restart.
    bool handleEvent(char event, Camera &camera);

  private:
    bool _lockLookAt = false;
    bool _lockLookFrom = false;
};

} // namespace RayTracer

#endif // CAMERA_CONTROLLER_HPP_
