/*
** EPITECH PROJECT, 2023
** raytracer
** File description:
** Color.hpp
*/

#ifndef COLOR_HPP_
#define COLOR_HPP_

#include "Vec3D.hpp"
#include <algorithm>
#include <sys/types.h>

namespace RayTracer
{

using Color3D = Vec3D;

class Color4D
{
  public:
    Color4D(void);
    Color4D(const float &x, const float &y, const float &z);
    Color4D(const Color3D &color);
    Color4D(const Color3D &color, const float &a);
    Color4D(const float &x, const float &y, const float &z, const float &w);
    ~Color4D(void);

    float getX(void) const;
    float getY(void) const;
    float getZ(void) const;
    float getW(void) const;

    void setX(const float x);
    void setY(const float y);
    void setZ(const float z);
    void setW(const float w);

    void clampColor(float min, float max);

    Color4D operator+(const Color4D &rhsVec4D) const;
    Color4D operator+(const float t) const;
    Color4D operator-(const float t) const;
    Color4D operator-(const Color4D &rhsVec4D) const;
    Color4D operator*(const Color4D &rhsVec4D) const;
    Color4D operator*(const float t) const;
    Color4D operator/(const Color4D &rhsVec4D) const;
    Color4D operator/(const float t) const;
    Color4D &operator+=(const Color4D &rhsVec4D);
    Color4D &operator-=(const Color4D &rhsVec4D);
    Color4D &operator*=(const Color4D &rhsVec4D);
    Color4D &operator*=(const float t);
    Color4D &operator/=(const Color4D &rhsVec4D);
    Color4D &operator/=(const float t);

    friend Color4D operator+(const float t, const Color4D &rhsVec4D);
    friend Color4D operator-(const float t, const Color4D &rhsVec4D);
    friend Color4D operator*(const float t, const Color4D &rhsVec4D);
    friend Color4D operator/(const float t, const Color4D &rhsVec4D);
    friend Color4D &operator*=(const float t, Color4D &rhsVec4D);
    friend Color4D &operator/=(const float t, Color4D &rhsVec4D);

  private:
    std::array<float, 4> _pos4D;
};

inline std::ostream &operator<<(std::ostream &os, const Color4D &color)
{
    return os << color.getX() << " " << color.getY() << " " << color.getZ();
}

inline float rgbToLinear(float rgb)
{
    if (!(rgb > 0.0f))
        return 0.0f;

    const float value = std::clamp(rgb, 0.0f, 255.0f) / 255.0f;

    return value <= 0.04045f ? value / 12.92f : std::pow((value + 0.055f) / 1.055f, 2.4f);
}

inline Color4D ColorRGB(const float &r, const float &g, const float &b, const float &a)
{
    return Color4D(rgbToLinear(r), rgbToLinear(g), rgbToLinear(b), a);
}

inline float colorLinearToGamma(float linearComponent)
{
    const float value = std::clamp(linearComponent, 0.0f, 1.0f);

    if (value <= 0.0031308f)
        return 12.92f * value;
    return 1.055f * std::pow(value, 1.0f / 2.4f) - 0.055f;
}

} // namespace RayTracer

#endif // COLOR_HPP_
