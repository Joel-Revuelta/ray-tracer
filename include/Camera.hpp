/*
** EPITECH PROJECT, 2023
** raytracer
** File description:
** Camera.hpp
*/

#ifndef CAMERA_HPP_
#define CAMERA_HPP_

#include "Point3D.hpp"
#include "Ray.hpp"
#include "Vec3D.hpp"

namespace RayTracer
{

class Camera
{
  public:
    Camera(void);
    Camera(const int &width, const int &height);
    Camera(const Vec3D &lookAt, const Vec3D &lookFrom, const float &fovDegree);
    Camera(const int &width, const int &height, const Vec3D &lookAt, const Vec3D &lookFrom, const float &fovDegree);
    ~Camera(void);

    void initialize(int imageWidth, int imageHeight, Point3D lookAt, Point3D lookFrom, float verticalFovDegree);

    void setImageWidth(const int imageWidth);
    void setImageHeight(const int imageHeight);
    void setLookFrom(const Point3D eyePoint);
    void setLookAt(const Point3D lookAt);
    void setAspectRatio(const double aspectRatio);
    void setFocalDistance(const int focalDistance); // Could be removed. Only used in initialize.
    void setViewportWidth(const double viewportWidth);
    void setViewportHeight(const double viewportHeight);
    void setInitialPixelPos(const Vec3D initialPixelPos);
    void setVerticalFovDegree(const float verticalFovDegree);     // Could be removed. Only used in initialize.
    void setPixelWidthDistance(const Vec3D pixelWidthDistance);   // Could be removed. Only used in initialize.
    void setPixelHeightDistance(const Vec3D pixelHeightDistance); // Could be removed. Only used in initialize.
    void setCamOrientation(const Vec3D camOrientation);           // Could be removed. Only used in initialize.
    void setCamViewUp(const Vec3D camViewUp);                     // Could be removed. Only used in initialize.
    void setCamViewRight(const Vec3D camViewRight);               // Could be removed. Only used in initialize.
    void setCamViewForward(const Vec3D camViewForward);           // Could be removed. Only used in initialize.

    int getImageWidth(void) const;
    int getImageHeight(void) const;
    Point3D getLookFrom(void) const;
    Point3D getLookAt(void) const;
    int getFocalDistance(void) const; // Could be removed. Only used in initialize.
    double getAspectRatio(void) const;
    double getViewportWidth(void) const;
    double getViewportHeight(void) const;
    Vec3D getInitialPixelPos(void) const;
    float getVerticalFovDegree(void) const;   // Could be removed. Only used in initialize.
    Vec3D getPixelWidthDistance(void) const;  // Could be removed. Only used in initialize.
    Vec3D getPixelHeightDistance(void) const; // Could be removed. Only used in initialize.
    Vec3D getCamOrientation(void) const;      // Could be removed. Only used in initialize.
    Vec3D getCamViewUp(void) const;           // Could be removed. Only used in initialize.
    Vec3D getCamViewRight(void) const;        // Could be removed. Only used in initialize.
    Vec3D getCamViewForward(void) const;      // Could be removed. Only used in initialize.

    Ray randomPixelRayCompute(uint32_t &seed, const float &x, const float &y) const;

  private:
    // Final image dimensions.
    int _imageWidth;
    int _imageHeight;

    // Distance between viewport and eye point.
    //? Note: Could be removed. Only used in initialize.
    int _focalDistance;

    // Aspect ratio of the camera width and height.
    double _aspectRatio;

    // Viewport dimensions. (Viewport is the area in the space representating the rendered result)
    double _viewportHeight;
    double _viewportWidth;

    // Camera fov in degrees.
    //? Note: Could be removed. Only used in initialize.
    float _fovDegree;

    // Camera orientation.
    //? Note: if (X == 0 && Y == 0 && Z == 0) error
    //? Note: Could be removed. Only used in initialize.
    Vec3D _camOrientation;

    // Where camera is looking at.
    //? Note: Z != 0
    //?  - Z < 0: camera is looking at the negative Z axis.
    //?  - Z > 0: camera is looking at the positive Z axis.
    Point3D _lookAt;

    // Where camera is looking from. (eye point rn)
    Point3D _lookFrom;

    // Camera view 3D vectors.
    //? Note: Could be removed. Only used in initialize.
    Vec3D _camViewUp;
    Vec3D _camViewRight;
    Vec3D _camViewForward;

    // Pixels information.
    //? Note: Could be removed. Only used in initialize.
    Vec3D _initialPixelPos;
    Vec3D _pixelWidthDistance;
    Vec3D _pixelHeightDistance;

    // The dimensions of the viewport are computed based on the aspect ratio of the camera. It has to follow that aspect
    // ratio because if not, the image will be distorted. ex: streched or compressed.
    double aspectRatioCompute(void);
    double viewportHeightCompute(void);
    double viewportWidthCompute(void);
};

} // namespace RayTracer

#endif // CAMERA_HPP_
