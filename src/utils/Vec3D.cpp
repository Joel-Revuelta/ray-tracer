/*
** EPITECH PROJECT, 2023
** raytracer
** File description:
** Vec3D.cpp
*/

#include "Vec3D.hpp"
#include "VectorIndexOutOfBounds.hpp"

using namespace RayTracer;

Vec3D::Vec3D(void) : _pos3D({0, 0, 0})
{
}

Vec3D::Vec3D(const float all) : _pos3D({all, all, all})
{
}

Vec3D::Vec3D(const float x, const float y, const float z) : _pos3D({x, y, z})
{
}

Vec3D::~Vec3D(void)
{
}

/**
 * Calculates the squared length of the vector.
 *
 * @return The squared length of the vector.
 */
float Vec3D::lengthSquared(void) const
{
    return _pos3D[0] * _pos3D[0] + _pos3D[1] * _pos3D[1] + _pos3D[2] * _pos3D[2];
}

/**
 * Calculates the length of the vector.
 *
 * @return The length of the vector.
 */
float Vec3D::length(void) const
{
    return sqrt(lengthSquared());
}

float Vec3D::getX(void) const
{
    return _pos3D[0];
}

float Vec3D::getY(void) const
{
    return _pos3D[1];
}

float Vec3D::getZ(void) const
{
    return _pos3D[2];
}

void Vec3D::setX(const float x)
{
    _pos3D[0] = x;
}

void Vec3D::setY(const float y)
{
    _pos3D[1] = y;
}

void Vec3D::setZ(const float z)
{
    _pos3D[2] = z;
}

Vec3D Vec3D::operator-(void) const
{
    return Vec3D(-_pos3D[0], -_pos3D[1], -_pos3D[2]);
}

float Vec3D::operator[](int i) const
{
    if (i < 0 || i >= 3)
    {
        throw VectorIndexOutOfBounds();
    }
    return _pos3D[i];
}

float &Vec3D::operator[](int i)
{
    if (i < 0 || i >= 3)
    {
        throw VectorIndexOutOfBounds();
    }
    return _pos3D[i];
}

Vec3D Vec3D::operator+(const Vec3D &rhsVec3D) const
{
    return Vec3D(_pos3D[0] + rhsVec3D.getX(), _pos3D[1] + rhsVec3D.getY(), _pos3D[2] + rhsVec3D.getZ());
}

Vec3D Vec3D::operator-(const Vec3D &rhsVec3D) const
{
    return Vec3D(_pos3D[0] - rhsVec3D.getX(), _pos3D[1] - rhsVec3D.getY(), _pos3D[2] - rhsVec3D.getZ());
}

Vec3D Vec3D::operator*(const Vec3D &rhsVec3D) const
{
    return Vec3D(_pos3D[0] * rhsVec3D.getX(), _pos3D[1] * rhsVec3D.getY(), _pos3D[2] * rhsVec3D.getZ());
}

Vec3D Vec3D::operator/(const Vec3D &rhsVec3D) const
{
    return Vec3D(_pos3D[0] / rhsVec3D.getX(), _pos3D[1] / rhsVec3D.getY(), _pos3D[2] / rhsVec3D.getZ());
}

Vec3D Vec3D::operator+(const float t) const
{
    return Vec3D(_pos3D[0] + t, _pos3D[1] + t, _pos3D[2] + t);
}

Vec3D Vec3D::operator-(const float t) const
{
    return Vec3D(_pos3D[0] - t, _pos3D[1] - t, _pos3D[2] - t);
}

Vec3D Vec3D::operator*(const float t) const
{
    return Vec3D(_pos3D[0] * t, _pos3D[1] * t, _pos3D[2] * t);
}

Vec3D Vec3D::operator/(const float t) const
{
    return Vec3D(_pos3D[0] / t, _pos3D[1] / t, _pos3D[2] / t);
}

Vec3D &Vec3D::operator+=(const Vec3D &rhsVec3D)
{
    _pos3D[0] += rhsVec3D.getX();
    _pos3D[1] += rhsVec3D.getY();
    _pos3D[2] += rhsVec3D.getZ();
    return *this;
}

Vec3D &Vec3D::operator-=(const Vec3D &rhsVec3D)
{
    _pos3D[0] -= rhsVec3D.getX();
    _pos3D[1] -= rhsVec3D.getY();
    _pos3D[2] -= rhsVec3D.getZ();
    return *this;
}

Vec3D &Vec3D::operator*=(const Vec3D &rhsVec3D)
{
    _pos3D[0] *= rhsVec3D.getX();
    _pos3D[1] *= rhsVec3D.getY();
    _pos3D[2] *= rhsVec3D.getZ();
    return *this;
}

Vec3D &Vec3D::operator/=(const Vec3D &rhsVec3D)
{
    _pos3D[0] /= rhsVec3D.getX();
    _pos3D[1] /= rhsVec3D.getY();
    _pos3D[2] /= rhsVec3D.getZ();
    return *this;
}

Vec3D &Vec3D::operator*=(const float t)
{
    _pos3D[0] *= t;
    _pos3D[1] *= t;
    _pos3D[2] *= t;
    return *this;
}

Vec3D &Vec3D::operator/=(const float t)
{
    _pos3D[0] /= t;
    _pos3D[1] /= t;
    _pos3D[2] /= t;
    return *this;
}

// Below are the friend non-member functions
namespace RayTracer
{

Vec3D operator+(const float t, const Vec3D &rhsVec3D)
{
    return Vec3D(rhsVec3D.getX() + t, rhsVec3D.getY() + t, rhsVec3D.getZ() + t);
}

Vec3D operator-(const float t, const Vec3D &rhsVec3D)
{
    return Vec3D(rhsVec3D.getX() - t, rhsVec3D.getY() - t, rhsVec3D.getZ() - t);
}

Vec3D operator*(const float t, const Vec3D &rhsVec3D)
{
    return Vec3D(rhsVec3D.getX() * t, rhsVec3D.getY() * t, rhsVec3D.getZ() * t);
}

Vec3D operator/(const float t, const Vec3D &rhsVec3D)
{
    return Vec3D(rhsVec3D.getX() / t, rhsVec3D.getY() / t, rhsVec3D.getZ() / t);
}

Vec3D &operator*=(const float t, Vec3D &rhsVec3D)
{
    rhsVec3D *= t;
    return rhsVec3D;
}

Vec3D &operator/=(const float t, Vec3D &rhsVec3D)
{
    rhsVec3D /= t;
    return rhsVec3D;
}

} // namespace RayTracer
