/*
** EPITECH PROJECT, 2023
** raytracer
** File description:
** Color.cpp
*/

#include "Color.hpp"

using namespace RayTracer;

Color4D::Color4D(void) : _pos4D({0, 0, 0, 0})
{
}
Color4D::Color4D(const float &x, const float &y, const float &z) : _pos4D({x, y, z, 1})
{
}
Color4D::Color4D(const Color3D &color) : _pos4D({color.getX(), color.getY(), color.getZ(), 1})
{
}
Color4D::Color4D(const Color3D &color, const float &a) : _pos4D({color.getX(), color.getY(), color.getZ(), a})
{
}
Color4D::Color4D(const float &x, const float &y, const float &z, const float &w) : _pos4D({x, y, z, w})
{
}
Color4D::~Color4D(void)
{
}

float Color4D::getX(void) const
{
    return _pos4D[0];
}

float Color4D::getY(void) const
{
    return _pos4D[1];
}

float Color4D::getZ(void) const
{
    return _pos4D[2];
}

float Color4D::getW(void) const
{
    return _pos4D[3];
}

void Color4D::setX(const float x)
{
    _pos4D[0] = x;
}

void Color4D::setY(const float y)
{
    _pos4D[1] = y;
}

void Color4D::setZ(const float z)
{
    _pos4D[2] = z;
}

void Color4D::setW(const float w)
{
    _pos4D[3] = w;
}

void Color4D::clampColor(float min, float max)
{
    _pos4D[0] = std::clamp(_pos4D[0], min, max);
    _pos4D[1] = std::clamp(_pos4D[1], min, max);
    _pos4D[2] = std::clamp(_pos4D[2], min, max);
    _pos4D[3] = std::clamp(_pos4D[3], min, max);
}

Color4D Color4D::operator+(const Color4D &rhsVec4D) const
{
    return Color4D(_pos4D[0] + rhsVec4D.getX(), _pos4D[1] + rhsVec4D.getY(), _pos4D[2] + rhsVec4D.getZ(),
                   _pos4D[3] + rhsVec4D.getW());
}

Color4D Color4D::operator-(const Color4D &rhsVec4D) const
{
    return Color4D(_pos4D[0] - rhsVec4D.getX(), _pos4D[1] - rhsVec4D.getY(), _pos4D[2] - rhsVec4D.getZ(),
                   _pos4D[3] - rhsVec4D.getW());
}

Color4D Color4D::operator*(const Color4D &rhsVec4D) const
{
    return Color4D(_pos4D[0] * rhsVec4D.getX(), _pos4D[1] * rhsVec4D.getY(), _pos4D[2] * rhsVec4D.getZ(),
                   _pos4D[3] * rhsVec4D.getW());
}

Color4D Color4D::operator/(const Color4D &rhsVec4D) const
{
    return Color4D(_pos4D[0] / rhsVec4D.getX(), _pos4D[1] / rhsVec4D.getY(), _pos4D[2] / rhsVec4D.getZ(),
                   _pos4D[3] / rhsVec4D.getW());
}

Color4D Color4D::operator+(const float t) const
{
    return Color4D(_pos4D[0] + t, _pos4D[1] + t, _pos4D[2] + t, _pos4D[3] + t);
}

Color4D Color4D::operator-(const float t) const
{
    return Color4D(_pos4D[0] - t, _pos4D[1] - t, _pos4D[2] - t, _pos4D[3] - t);
}

Color4D Color4D::operator*(const float t) const
{
    return Color4D(_pos4D[0] * t, _pos4D[1] * t, _pos4D[2] * t, _pos4D[3] * t);
}

Color4D Color4D::operator/(const float t) const
{
    return Color4D(_pos4D[0] / t, _pos4D[1] / t, _pos4D[2] / t, _pos4D[3] / t);
}

Color4D &Color4D::operator+=(const Color4D &rhsVec4D)
{
    _pos4D[0] += rhsVec4D.getX();
    _pos4D[1] += rhsVec4D.getY();
    _pos4D[2] += rhsVec4D.getZ();
    _pos4D[3] += rhsVec4D.getW();
    return *this;
}

Color4D &Color4D::operator-=(const Color4D &rhsVec4D)
{
    _pos4D[0] -= rhsVec4D.getX();
    _pos4D[1] -= rhsVec4D.getY();
    _pos4D[2] -= rhsVec4D.getZ();
    _pos4D[3] -= rhsVec4D.getW();
    return *this;
}

Color4D &Color4D::operator*=(const Color4D &rhsVec4D)
{
    _pos4D[0] *= rhsVec4D.getX();
    _pos4D[1] *= rhsVec4D.getY();
    _pos4D[2] *= rhsVec4D.getZ();
    _pos4D[3] *= rhsVec4D.getW();
    return *this;
}

Color4D &Color4D::operator/=(const Color4D &rhsVec4D)
{
    _pos4D[0] /= rhsVec4D.getX();
    _pos4D[1] /= rhsVec4D.getY();
    _pos4D[2] /= rhsVec4D.getZ();
    _pos4D[3] /= rhsVec4D.getW();
    return *this;
}

Color4D &Color4D::operator*=(const float t)
{
    _pos4D[0] *= t;
    _pos4D[1] *= t;
    _pos4D[2] *= t;
    _pos4D[3] *= t;
    return *this;
}

Color4D &Color4D::operator/=(const float t)
{
    _pos4D[0] /= t;
    _pos4D[1] /= t;
    _pos4D[2] /= t;
    _pos4D[3] /= t;
    return *this;
}

namespace RayTracer
{

Color4D operator+(const float t, const Color4D &rhsVec4D)
{
    return Color4D(rhsVec4D.getX() + t, rhsVec4D.getY() + t, rhsVec4D.getZ() + t, rhsVec4D.getW() + t);
}

Color4D operator-(const float t, const Color4D &rhsVec4D)
{
    return Color4D(rhsVec4D.getX() - t, rhsVec4D.getY() - t, rhsVec4D.getZ() - t, rhsVec4D.getW() - t);
}

Color4D operator*(const float t, const Color4D &rhsVec4D)
{
    return Color4D(rhsVec4D.getX() * t, rhsVec4D.getY() * t, rhsVec4D.getZ() * t, rhsVec4D.getW() * t);
}

Color4D operator/(const float t, const Color4D &rhsVec4D)
{
    return Color4D(rhsVec4D.getX() / t, rhsVec4D.getY() / t, rhsVec4D.getZ() / t, rhsVec4D.getW() / t);
}

Color4D &operator*=(const float t, Color4D &rhsVec4D)
{
    rhsVec4D *= t;
    return rhsVec4D;
}

Color4D &operator/=(const float t, Color4D &rhsVec4D)
{
    rhsVec4D /= t;
    return rhsVec4D;
}
} // namespace RayTracer
