/*
** EPITECH PROJECT, 2023
** raytracer
** File description:
** Camera.cpp
*/

#include "Camera.hpp"
#include "Point3D.hpp"
#include "Ray.hpp"
#include "Utils.hpp"
#include "Vec3D.hpp"
#include <cmath>
#include <cstdlib>
#include <iostream>

using namespace RayTracer;

Camera::Camera(void)
{
    initialize(960, 540, Vec3D(0, 0, 1), Vec3D(), 90.0f);
}

Camera::Camera(const int &width, const int &height)
{
    initialize(width, height, Vec3D(0, 0, 1), Vec3D(), 90.0f);
}

Camera::Camera(const Vec3D &lookAt, const Vec3D &lookFrom, const float &verticalFovDegree)
{
    initialize(960, 540, lookAt, lookFrom, verticalFovDegree);
}

Camera::Camera(const int &width, const int &height, const Vec3D &lookAt, const Vec3D &lookFrom,
               const float &verticalFovDegree)
{
    initialize(width, height, lookAt, lookFrom, verticalFovDegree);
}

Camera::~Camera(void)
{
}

/**
 * Initializes the camera by computing the necessary vectors and positions for rendering.
 */
void Camera::initialize(int imageWidth, int imageHeight, Point3D lookAt, Point3D lookFrom, float verticalFovDegree)
{
    // Set constructor values for the camera.
    setImageWidth(imageWidth);
    setImageHeight(imageHeight);
    setVerticalFovDegree(verticalFovDegree);
    setLookFrom(lookFrom);
    setLookAt(lookAt);
    setFocalDistance((lookFrom - lookAt).length());
    setAspectRatio(aspectRatioCompute());

    // Set default orientation, ooking horizontally.
    setCamOrientation(Vec3D(0, 1, 0));

    float verticalFovRadians = Utils::numericDegreesToRadians<float>(getVerticalFovDegree());
    float fovHeight = tan(verticalFovRadians / 2);

    // Set the viewport height and width.
    setViewportHeight(2 * fovHeight * getFocalDistance());
    setViewportWidth(viewportWidthCompute());

    // Compute the camera view vectors.
    setCamViewForward(vec3DNormalize(lookFrom - lookAt));
    setCamViewRight(vec3DNormalize(vec3DCross(getCamOrientation(), getCamViewForward())));
    setCamViewUp(vec3DCross(getCamViewForward(), getCamViewRight()));

    // Compute vectors across the viewport.
    Vec3D viewportWidthVector = getViewportWidth() * getCamViewRight();
    Vec3D viewportHeightVector = getViewportHeight() * -getCamViewUp();

    // Compute the upper left corner world coordinates of the viewport.
    Vec3D viewportUpperLeft =
        getLookFrom() - (getFocalDistance() * getCamViewForward()) - viewportWidthVector / 2 - viewportHeightVector / 2;

    // Compute the distance between pixels.
    setPixelWidthDistance(viewportWidthVector / getImageWidth());
    setPixelHeightDistance(viewportHeightVector / getImageHeight());

    // Compute the initial pixel position. Considering EQUAL PIXEL WIDTHS AND HEIGHTS.
    setInitialPixelPos(viewportUpperLeft + 0.5 * (getPixelWidthDistance() + getPixelHeightDistance()));
}

/**
 * Computes a random ray for a given pixel on the camera's image plane.
 *
 * @param seed The seed value for the random number generator.
 * @param x The x-coordinate of the pixel on the image plane.
 * @param y The y-coordinate of the pixel on the image plane.
 * @return A Ray object representing the computed random ray.
 */
Ray Camera::randomPixelRayCompute(uint32_t &seed, const float &x, const float &y) const
{
    // Create seed for random number generation.

    // Compute a random position (of the real world) inside the pixel.
    //? Note: Weird things happening here, if using numericGeneratePseudoRandom, image seems rendered correctly but less
    //? quality. If using numericGenerateRandom, image seems rendered with more quality but with some bias.
    (void)seed;
    // Vec3D pixelRandomPos = Vec3D(Utils::numericGenerateRandom<float>(0.0, 0.999) - 0.5,
    // Utils::numericGenerateRandom<float>(0.0, 0.999) - 0.5, 0);
    Vec3D pixelRandomPos = Vec3D(Utils::numericGeneratePseudoRandom<float>(seed, 0.0, 0.999) - 0.5,
                                 Utils::numericGeneratePseudoRandom<float>(seed, 0.0, 0.999) - 0.5, 0);

    // Compute the real world position of the pixel.
    Vec3D positionToHit = getInitialPixelPos() + ((x + pixelRandomPos.getX()) * getPixelWidthDistance()) +
                          ((y + pixelRandomPos.getY()) * getPixelHeightDistance());

    Vec3D rayDirection = positionToHit - getLookFrom();

    // Return the ray.
    return Ray(getLookFrom(), rayDirection);
}

/**
 * Computes the aspect ratio of the camera based on the given width and height.
 *
 * @param width The width of the camera viewport.
 * @param height The height of the camera viewport.
 * @return The aspect ratio of the camera.
 */
double Camera::aspectRatioCompute()
{
    // Cast to double to avoid integer division and instead perform floating point division.
    return (double)getImageWidth() / (double)getImageHeight();
}

/**
 * Computes the height of the viewport based on the given height and aspect ratio.
 *
 * @param height The height of the viewport.
 * @param aspectRatio The aspect ratio of the viewport.
 * @return The computed height of the viewport.
 */
double Camera::viewportHeightCompute(void)
{
    return (double)getImageHeight() / getAspectRatio();
}

/**
 * Computes the width of the viewport based on the given aspect ratio and viewport height.
 *
 * @param aspectRatio The aspect ratio of the viewport.
 * @param viewportHeight The height of the viewport.
 * @return The width of the viewport.
 */
double Camera::viewportWidthCompute(void)
{
    return getViewportHeight() * (double)getImageWidth() / getImageHeight();
}

void Camera::setImageWidth(const int imageWidth)
{
    _imageWidth = imageWidth;
}

void Camera::setImageHeight(const int imageHeight)
{
    _imageHeight = imageHeight;
}

void Camera::setLookFrom(const Vec3D eyePoint)
{
    _lookFrom = eyePoint;
}

void Camera::setLookAt(const Vec3D lookAt)
{
    _lookAt = lookAt;
}

void Camera::setFocalDistance(const int focalDistance)
{
    _focalDistance = focalDistance;
}

void Camera::setAspectRatio(const double aspectRatio)
{
    _aspectRatio = aspectRatio;
}

void Camera::setViewportHeight(const double viewportHeight)
{
    _viewportHeight = viewportHeight;
}

void Camera::setViewportWidth(const double viewportWidth)
{
    _viewportWidth = viewportWidth;
}

void Camera::setInitialPixelPos(const Vec3D initialPixelPos)
{
    _initialPixelPos = initialPixelPos;
}

void Camera::setPixelWidthDistance(const Vec3D pixelWidthDistance)
{
    _pixelWidthDistance = pixelWidthDistance;
}

void Camera::setPixelHeightDistance(const Vec3D pixelHeightDistance)
{
    _pixelHeightDistance = pixelHeightDistance;
}

void Camera::setVerticalFovDegree(const float verticalFovDegree)
{
    _fovDegree = verticalFovDegree;
}

void Camera::setCamOrientation(const Vec3D camOrientation)
{
    _camOrientation = camOrientation;
}

void Camera::setCamViewUp(const Vec3D camViewUp)
{
    _camViewUp = camViewUp;
}

void Camera::setCamViewRight(const Vec3D camViewRight)
{
    _camViewRight = camViewRight;
}

void Camera::setCamViewForward(const Vec3D camViewForward)
{
    _camViewForward = camViewForward;
}

int Camera::getImageWidth(void) const
{
    return _imageWidth;
}

int Camera::getImageHeight(void) const
{
    return _imageHeight;
}

Vec3D Camera::getLookFrom(void) const
{
    return _lookFrom;
}

Vec3D Camera::getLookAt(void) const
{
    return _lookAt;
}

int Camera::getFocalDistance(void) const
{
    return _focalDistance;
}

double Camera::getAspectRatio(void) const
{
    return _aspectRatio;
}

double Camera::getViewportHeight(void) const
{
    return _viewportHeight;
}

double Camera::getViewportWidth(void) const
{
    return _viewportWidth;
}

Vec3D Camera::getInitialPixelPos(void) const
{
    return _initialPixelPos;
}

Vec3D Camera::getPixelWidthDistance(void) const
{
    return _pixelWidthDistance;
}

Vec3D Camera::getPixelHeightDistance(void) const
{
    return _pixelHeightDistance;
}

float Camera::getVerticalFovDegree(void) const
{
    return _fovDegree;
}

Vec3D Camera::getCamOrientation(void) const
{
    return _camOrientation;
}

Vec3D Camera::getCamViewUp(void) const
{
    return _camViewUp;
}

Vec3D Camera::getCamViewRight(void) const
{
    return _camViewRight;
}

Vec3D Camera::getCamViewForward(void) const
{
    return _camViewForward;
}
