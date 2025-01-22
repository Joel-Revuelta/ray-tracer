/*
** EPITECH PROJECT, 2023
** raytracer
** File description:
** Ray.hpp
*/

#ifndef RAY_HPP_
#define RAY_HPP_

#include "Point3D.hpp"
#include "Vec3D.hpp"

namespace RayTracer
{

class Ray
{
  public:
    Ray(void);
    Ray(const Point3D &origin, const Vec3D &direction);
    ~Ray(void);

    Point3D getOrigin(void) const;
    Vec3D getDirection(void) const;

    void setOrigin(const Point3D &origin);
    void setDirection(const Vec3D &direction);

    Point3D at(float t) const;

    constexpr static const float EPSILON = 0.0001f;

  private:
    Point3D _origin;
    Vec3D _direction;
};

inline std::ostream &operator<<(std::ostream &os, const Ray &ray)
{
    os << "Ray: " << std::endl;
    os << "  - Origin: " << ray.getOrigin() << std::endl;
    os << "  - Direction: " << ray.getDirection() << std::endl;
    return os;
}

} // namespace RayTracer

#endif // RAY_HPP_
