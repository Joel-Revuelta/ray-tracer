/*
** EPITECH PROJECT, 2023
** raytracer
** File description:
** Vec3D.hpp
*/

#ifndef VEC3D_HPP_
#define VEC3D_HPP_

#include "Utils.hpp"
#include <array>
#include <cmath>
#include <ostream>

namespace RayTracer
{

class Vec3D;

class Vec3D
{
  public:
    Vec3D(void);
    Vec3D(const float all);
    Vec3D(const float x, const float y, const float z);
    ~Vec3D(void);

    float length(void) const;
    float lengthSquared(void) const;

    float getX(void) const;
    float getY(void) const;
    float getZ(void) const;

    void setX(const float x);
    void setY(const float y);
    void setZ(const float z);

    // Return new Vec3D with opposite coordinates
    Vec3D operator-(void) const;

    // Return unmutable value of the i-th coordinate of pos3D.
    // Will be called when using [] on a const Vec3D
    float operator[](int i) const;

    // Return mutable value of the i-th coordinate of pos3D
    // Will be called when using [] on a non-const Vec3D
    float &operator[](int i);

    Vec3D operator+(const Vec3D &rhsVec3D) const;
    Vec3D operator+(const float t) const;
    Vec3D operator-(const Vec3D &rhsVec3D) const;
    Vec3D operator-(const float t) const;
    Vec3D operator*(const Vec3D &rhsVec3D) const;
    Vec3D operator*(const float t) const;
    Vec3D operator/(const Vec3D &rhsVec3D) const;
    Vec3D operator/(const float t) const;
    Vec3D &operator+=(const Vec3D &rhsVec3D);
    Vec3D &operator-=(const Vec3D &rhsVec3D);
    Vec3D &operator*=(const Vec3D &rhsVec3D);
    Vec3D &operator*=(const float t);
    Vec3D &operator/=(const Vec3D &rhsVec3D);
    Vec3D &operator/=(const float t);

    // Use friend keyword to allow operator overloading for non-member functions
    friend Vec3D operator+(const float t, const Vec3D &rhsVec3D);
    friend Vec3D operator-(const float t, const Vec3D &rhsVec3D);
    friend Vec3D operator*(const float t, const Vec3D &rhsVec3D);
    friend Vec3D operator/(const float t, const Vec3D &rhsVec3D);
    friend Vec3D &operator*=(const float t, Vec3D &rhsVec3D);
    friend Vec3D &operator/=(const float t, Vec3D &rhsVec3D);

  private:
    std::array<float, 3> _pos3D;
};

// Print Vec3D such as "x y z"
inline std::ostream &operator<<(std::ostream &os, const Vec3D &vec)
{
    return os << vec[0] << " " << vec[1] << " " << vec[2];
}

/**
 * Calculates the dot product of two 3D vectors.
 *
 * @param u The first vector.
 * @param v The second vector.
 * @return The dot product of the two vectors.
 */
inline float vec3DDot(const Vec3D &u, const Vec3D &v)
{
    return u[0] * v[0] + u[1] * v[1] + u[2] * v[2];
}

/**
 * Calculates the cross product of two 3D vectors.
 *
 * @param u The first vector.
 * @param v The second vector.
 * @return The cross product of the two vectors.
 */
inline Vec3D vec3DCross(const Vec3D &u, const Vec3D &v)
{
    return Vec3D(u[1] * v[2] - u[2] * v[1], u[2] * v[0] - u[0] * v[2], u[0] * v[1] - u[1] * v[0]);
}

/**
 * Normalizes a 3D vector.
 *
 * @param v The vector to be normalized.
 * @return The normalized vector.
 */
inline Vec3D vec3DNormalize(const Vec3D &v)
{
    return v / v.length();
}

/**
 * Generates a random 3D vector with each component within the specified range.
 *
 * @param min The minimum value for each component of the vector.
 * @param max The maximum value for each component of the vector.
 * @return A randomly generated 3D vector.
 */
inline Vec3D vec3DRandom(float min, float max)
{
    return Vec3D(Utils::numericGenerateRandom<float>(min, max), Utils::numericGenerateRandom<float>(min, max),
                 Utils::numericGenerateRandom<float>(min, max));
}

/**
 * Generates a random 3D vector within a unit sphere.
 *
 * Generated via rejection algorithm. CAN BE CHANGED TO FASTER ALGORITHM.
 *
 * @return A randomly generated Vec3D within a unit sphere.
 */
inline Vec3D vec3DRandomOnUnitSphere(void)
{
    while (true)
    {
        Vec3D v = vec3DRandom(-1, 1);
        if (v.lengthSquared() >= 1)
            continue;
        return v;
    }
}

/**
 * Generates a random normalized vector on a 3D sphere space.
 *
 * @return A randomly generated unit vector.
 */
inline Vec3D vec3DRandomSphereUnit(void)
{
    return vec3DNormalize(vec3DRandomOnUnitSphere());
}

/**
 * Generates a random vector in the hemisphere defined by the given surface normal.
 *
 * @param surfaceNormal The surface normal vector.
 * @return A random vector in the hemisphere.
 */
inline Vec3D vec3DRandomOnHemisphere(const Vec3D &surfaceNormal)
{
    Vec3D onUnitSphere = vec3DRandomOnUnitSphere();

    // Ray inside the hemisphere
    if (vec3DDot(onUnitSphere, surfaceNormal) > 0.0)
        return onUnitSphere;
    else // Ray outside the hemisphere
        return -onUnitSphere;
}

/**
 * Checks if a given Vec3D is near zero.
 *
 * This function checks if the magnitude of the given Vec3D is very close to zero,
 * within a small tolerance value. It is useful for determining if a vector is
 * effectively zero, taking into account floating-point precision errors.
 *
 * @param v The Vec3D to check.
 * @return True if the magnitude of the Vec3D is near zero, false otherwise.
 */
inline bool vec3DIsNearZero(const Vec3D &v)
{
    return (fabs(v[0]) < 0.00000001) && (fabs(v[1]) < 0.00000001) && (fabs(v[2]) < 0.00000001);
}

/**
 * Calculates the reflection of a vector `incidentVector` against a surface with normal vector `normalVector`.
 *
 * This is a general material function. Reflection formula: R = I - 2 * dot(I, N) * N
 *
 * @param v The incident vector.
 * @param n The surface normal vector.
 * @return The reflected vector.
 */
inline Vec3D reflect(const Vec3D &incidentVector, const Vec3D &normalVector)
{
    return incidentVector - 2 * vec3DDot(incidentVector, normalVector) * normalVector;
}

} // namespace RayTracer

#endif // VEC3D_HPP_
