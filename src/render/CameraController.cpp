/*
** EPITECH PROJECT, 2024
** raytracer
** File description:
** CameraController
*/

#include "plugins/renderers/CameraController.hpp"
#include "Camera.hpp"
#include "Vec3D.hpp"
#include <stdexcept>
#include <unordered_map>
#include <utility>

using namespace RayTracer;

bool CameraController::handleEvent(char event, Camera &camera)
{
    if (!event)
        return false;

    if (event == 'x')
    {
        _lockLookAt = !_lockLookAt;
        return false;
    }
    if (event == 'c')
    {
        _lockLookFrom = !_lockLookFrom;
        return false;
    }

    // key, lookFrom offset, lookAt offset
    const std::unordered_map<char, std::pair<Vec3D, Vec3D>> keyMap = {
        {'w', {Vec3D(0, 0, 1), Vec3D(0, 0, 1)}}, {'s', {Vec3D(0, 0, -1), Vec3D(0, 0, -1)}},
        {'a', {Vec3D(1, 0, 0), Vec3D(1, 0, 0)}}, {'d', {Vec3D(-1, 0, 0), Vec3D(-1, 0, 0)}},
        {'q', {Vec3D(0, 1, 0), Vec3D(0, 1, 0)}}, {'e', {Vec3D(0, -1, 0), Vec3D(0, -1, 0)}},
        {'r', {Vec3D(0, 0, 0), Vec3D(0, 0, 0)}},
    };

    auto it = keyMap.find(event);
    if (it == keyMap.end())
        return false;

    const auto &move = it->second;
    const Vec3D newLookFrom = _lockLookFrom ? camera.getLookFrom() : camera.getLookFrom() + move.first;
    const Vec3D newLookAt = _lockLookAt ? camera.getLookAt() : camera.getLookAt() + move.second;

    try
    {
        camera.initialize(camera.getImageWidth(), camera.getImageHeight(), newLookAt, newLookFrom,
                          camera.getVerticalFovDegree());
    }
    catch (const std::invalid_argument &)
    {
        // Ignore a movement that would put the camera exactly on its
        // look-at point.
        return false;
    }

    return true;
}
