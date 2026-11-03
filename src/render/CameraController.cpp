/*
** EPITECH PROJECT, 2024
** raytracer
** File description:
** CameraController
*/

#include "plugins/renderers/CameraController.hpp"
#include "Camera.hpp"
#include "Utils.hpp"
#include "Vec3D.hpp"
#include <algorithm>
#include <cmath>
#include <stdexcept>

using namespace RayTracer;

namespace
{
constexpr float MOVE_STEP = 1.0f;
constexpr float LOOK_STEP_DEGREES = 4.0f;
constexpr float FOV_STEP_DEGREES = 2.0f;
constexpr float MIN_FOV_DEGREES = 10.0f;
constexpr float MAX_FOV_DEGREES = 170.0f;
constexpr float MAX_PITCH_DEGREES = 89.0f;
const Vec3D WORLD_UP(0.0f, 1.0f, 0.0f);

Vec3D rotateAroundAxis(const Vec3D &vector, const Vec3D &axis, float radians)
{
    const Vec3D normal = vec3DNormalize(axis);

    if (vec3DIsNearZero(normal))
        return vector;

    const float cosine = std::cos(radians);
    const float sine = std::sin(radians);

    return vector * cosine + vec3DCross(normal, vector) * sine + normal * vec3DDot(normal, vector) * (1.0f - cosine);
}
} // namespace

void CameraController::captureInitial(const Camera &camera)
{
    _initialLookFrom = camera.getLookFrom();
    _initialLookAt = camera.getLookAt();
    _initialFov = camera.getVerticalFovDegree();
    _hasInitialState = true;
}

bool CameraController::isCameraEvent(char event) const
{
    switch (event)
    {
    case 'w':
    case 'a':
    case 's':
    case 'd':
    case 'q':
    case 'e':
    case 'i':
    case 'j':
    case 'k':
    case 'l':
    case 'r':
    case '+':
    case '=':
    case '-':
    case '_':
        return true;
    default:
        return false;
    }
}

bool CameraController::handleEvent(char event, Camera &camera)
{
    if (!event)
        return false;

    if (!_hasInitialState)
        captureInitial(camera);

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
    if (event == 'r')
        return updateCamera(camera, _initialLookFrom, _initialLookAt, _initialFov);

    if (event == '+' || event == '=')
        return applyFov(camera, -FOV_STEP_DEGREES);
    if (event == '-' || event == '_')
        return applyFov(camera, FOV_STEP_DEGREES);

    if (event == 'i' || event == 'k' || event == 'j' || event == 'l')
        return applyLook(event, camera);

    return applyMovement(event, camera);
}

bool CameraController::applyMovement(char event, Camera &camera)
{
    Vec3D right = vec3DNormalize(camera.getCamViewRight());

    if (vec3DIsNearZero(right))
        right = Vec3D(1.0f, 0.0f, 0.0f);

    const Vec3D view = camera.getLookAt() - camera.getLookFrom();
    Vec3D forward(view.getX(), 0.0f, view.getZ());

    if (vec3DIsNearZero(forward))
        forward = vec3DCross(WORLD_UP, right);

    forward = vec3DNormalize(forward);

    if (vec3DIsNearZero(forward))
        forward = Vec3D(0.0f, 0.0f, 1.0f);

    Vec3D offset(0.0f);

    switch (event)
    {
    case 'w':
        offset = forward * MOVE_STEP;
        break;
    case 's':
        offset = -forward * MOVE_STEP;
        break;
    case 'd':
        offset = right * MOVE_STEP;
        break;
    case 'a':
        offset = -right * MOVE_STEP;
        break;
    case 'q':
        offset = WORLD_UP * MOVE_STEP;
        break;
    case 'e':
        offset = -WORLD_UP * MOVE_STEP;
        break;
    default:
        return false;
    }

    if (_lockLookAt && _lockLookFrom)
        return false;

    const Vec3D newLookFrom = _lockLookFrom ? camera.getLookFrom() : camera.getLookFrom() + offset;
    const Vec3D newLookAt = _lockLookAt ? camera.getLookAt() : camera.getLookAt() + offset;

    return updateCamera(camera, newLookFrom, newLookAt, camera.getVerticalFovDegree());
}

bool CameraController::applyLook(char event, Camera &camera)
{
    const Vec3D lookFrom = camera.getLookFrom();
    const Vec3D lookAt = camera.getLookAt();
    Vec3D offset = lookAt - lookFrom;
    const float distance = offset.length();

    if (distance < 1e-6f)
        return false;

    const float step = Utils::numericDegreesToRadians<float>(LOOK_STEP_DEGREES);
    Vec3D rotated = offset;

    if (event == 'j' || event == 'l')
    {
        const float yaw = event == 'j' ? step : -step;

        rotated = rotateAroundAxis(offset, WORLD_UP, yaw);
    }
    else if (event == 'i' || event == 'k')
    {
        Vec3D right = vec3DCross(offset, WORLD_UP);

        if (vec3DIsNearZero(right))
            right = camera.getCamViewRight();
        if (vec3DIsNearZero(right))
            return false;

        const float currentPitch = std::asin(std::clamp(offset.getY() / distance, -1.0f, 1.0f));
        const float pitchStep = event == 'i' ? step : -step;
        const float maxPitch = Utils::numericDegreesToRadians<float>(MAX_PITCH_DEGREES);

        if (std::fabs(currentPitch + pitchStep) > maxPitch)
            return false;

        rotated = rotateAroundAxis(offset, right, pitchStep);
    }
    else
        return false;

    if (_lockLookAt && _lockLookFrom)
        return false;

    if (_lockLookAt)
        return updateCamera(camera, lookAt - rotated, lookAt, camera.getVerticalFovDegree());

    return updateCamera(camera, lookFrom, lookFrom + rotated, camera.getVerticalFovDegree());
}

bool CameraController::applyFov(Camera &camera, float deltaDegrees)
{
    const float current = camera.getVerticalFovDegree();
    const float next = std::clamp(current + deltaDegrees, MIN_FOV_DEGREES, MAX_FOV_DEGREES);

    if (std::fabs(next - current) < 0.0001f)
        return false;

    return updateCamera(camera, camera.getLookFrom(), camera.getLookAt(), next);
}

bool CameraController::updateCamera(Camera &camera, const Vec3D &lookFrom, const Vec3D &lookAt, float fov)
{
    try
    {
        camera.initialize(camera.getImageWidth(), camera.getImageHeight(), lookAt, lookFrom, fov);
    }
    catch (const std::invalid_argument &)
    {
        // Ignore a movement that would put the camera exactly on its
        // look-at point or use an invalid field of view.
        return false;
    }

    return true;
}
