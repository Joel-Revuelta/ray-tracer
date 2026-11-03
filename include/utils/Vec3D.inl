/*
** EPITECH PROJECT, 2023
** raytracer
** File description:
** Vec3D inline implementation
*/

#ifndef VEC3D_INL_
#define VEC3D_INL_

#include "VectorIndexOutOfBounds.hpp"
#include <cmath>

namespace RayTracer
{

inline Vec3D::Vec3D(void) : _pos3D({0, 0, 0})
{
}

inline Vec3D::Vec3D(const float all) : _pos3D({all, all, all})
{
}

inline Vec3D::Vec3D(const float x, const float y, const float z) : _pos3D({x, y, z})
{
}

inline Vec3D::~Vec3D(void)
{
}

inline float Vec3D::lengthSquared(void) const
{
    return _pos3D[0] * _pos3D[0] + _pos3D[1] * _pos3D[1] + _pos3D[2] * _pos3D[2];
}

inline float Vec3D::length(void) const
{
    return std::sqrt(lengthSquared());
}

inline float Vec3D::getX(void) const
{
    return _pos3D[0];
}

inline float Vec3D::getY(void) const
{
    return _pos3D[1];
}

inline float Vec3D::getZ(void) const
{
    return _pos3D[2];
}

inline void Vec3D::setX(const float x)
{
    _pos3D[0] = x;
}

inline void Vec3D::setY(const float y)
{
    _pos3D[1] = y;
}

inline void Vec3D::setZ(const float z)
{
    _pos3D[2] = z;
}

inline Vec3D Vec3D::operator-(void) const
{
    return Vec3D(-_pos3D[0], -_pos3D[1], -_pos3D[2]);
}

inline float Vec3D::operator[](int i) const
{
    if (i < 0 || i >= 3)
        throw VectorIndexOutOfBounds();
    return _pos3D[i];
}

inline float &Vec3D::operator[](int i)
{
    if (i < 0 || i >= 3)
        throw VectorIndexOutOfBounds();
    return _pos3D[i];
}

inline Vec3D Vec3D::operator+(const Vec3D &rhsVec3D) const
{
    return Vec3D(_pos3D[0] + rhsVec3D.getX(), _pos3D[1] + rhsVec3D.getY(), _pos3D[2] + rhsVec3D.getZ());
}

inline Vec3D Vec3D::operator-(const Vec3D &rhsVec3D) const
{
    return Vec3D(_pos3D[0] - rhsVec3D.getX(), _pos3D[1] - rhsVec3D.getY(), _pos3D[2] - rhsVec3D.getZ());
}

inline Vec3D Vec3D::operator*(const Vec3D &rhsVec3D) const
{
    return Vec3D(_pos3D[0] * rhsVec3D.getX(), _pos3D[1] * rhsVec3D.getY(), _pos3D[2] * rhsVec3D.getZ());
}

inline Vec3D Vec3D::operator/(const Vec3D &rhsVec3D) const
{
    return Vec3D(_pos3D[0] / rhsVec3D.getX(), _pos3D[1] / rhsVec3D.getY(), _pos3D[2] / rhsVec3D.getZ());
}

inline Vec3D Vec3D::operator+(const float t) const
{
    return Vec3D(_pos3D[0] + t, _pos3D[1] + t, _pos3D[2] + t);
}

inline Vec3D Vec3D::operator-(const float t) const
{
    return Vec3D(_pos3D[0] - t, _pos3D[1] - t, _pos3D[2] - t);
}

inline Vec3D Vec3D::operator*(const float t) const
{
    return Vec3D(_pos3D[0] * t, _pos3D[1] * t, _pos3D[2] * t);
}

inline Vec3D Vec3D::operator/(const float t) const
{
    return Vec3D(_pos3D[0] / t, _pos3D[1] / t, _pos3D[2] / t);
}

inline Vec3D &Vec3D::operator+=(const Vec3D &rhsVec3D)
{
    _pos3D[0] += rhsVec3D.getX();
    _pos3D[1] += rhsVec3D.getY();
    _pos3D[2] += rhsVec3D.getZ();
    return *this;
}

inline Vec3D &Vec3D::operator-=(const Vec3D &rhsVec3D)
{
    _pos3D[0] -= rhsVec3D.getX();
    _pos3D[1] -= rhsVec3D.getY();
    _pos3D[2] -= rhsVec3D.getZ();
    return *this;
}

inline Vec3D &Vec3D::operator*=(const Vec3D &rhsVec3D)
{
    _pos3D[0] *= rhsVec3D.getX();
    _pos3D[1] *= rhsVec3D.getY();
    _pos3D[2] *= rhsVec3D.getZ();
    return *this;
}

inline Vec3D &Vec3D::operator/=(const Vec3D &rhsVec3D)
{
    _pos3D[0] /= rhsVec3D.getX();
    _pos3D[1] /= rhsVec3D.getY();
    _pos3D[2] /= rhsVec3D.getZ();
    return *this;
}

inline Vec3D &Vec3D::operator*=(const float t)
{
    _pos3D[0] *= t;
    _pos3D[1] *= t;
    _pos3D[2] *= t;
    return *this;
}

inline Vec3D &Vec3D::operator/=(const float t)
{
    _pos3D[0] /= t;
    _pos3D[1] /= t;
    _pos3D[2] /= t;
    return *this;
}

inline Vec3D operator+(const float t, const Vec3D &rhsVec3D)
{
    return Vec3D(rhsVec3D.getX() + t, rhsVec3D.getY() + t, rhsVec3D.getZ() + t);
}

inline Vec3D operator-(const float t, const Vec3D &rhsVec3D)
{
    return Vec3D(rhsVec3D.getX() - t, rhsVec3D.getY() - t, rhsVec3D.getZ() - t);
}

inline Vec3D operator*(const float t, const Vec3D &rhsVec3D)
{
    return Vec3D(rhsVec3D.getX() * t, rhsVec3D.getY() * t, rhsVec3D.getZ() * t);
}

inline Vec3D operator/(const float t, const Vec3D &rhsVec3D)
{
    return Vec3D(rhsVec3D.getX() / t, rhsVec3D.getY() / t, rhsVec3D.getZ() / t);
}

inline Vec3D &operator*=(const float t, Vec3D &rhsVec3D)
{
    rhsVec3D *= t;
    return rhsVec3D;
}

inline Vec3D &operator/=(const float t, Vec3D &rhsVec3D)
{
    rhsVec3D /= t;
    return rhsVec3D;
}

} // namespace RayTracer

#endif // VEC3D_INL_
