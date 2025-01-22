/*
** EPITECH PROJECT, 2023
** raytracer
** File description:
** HitPayload.hpp
*/

#ifndef HIT_PAYLOAD_HPP_
#define HIT_PAYLOAD_HPP_

#include "IMaterial.hpp"
#include "Ray.hpp"
#include "Vec3D.hpp"
#include <memory>
#include <ostream>

namespace RayTracer
{

class HitPayload
{
  public:
    HitPayload(void);
    ~HitPayload(void);

    void setHitIndex(const int &objectIndex);
    void setHitNormal(const Vec3D &_sceneNormal);
    void setHitDistance(const float &_sceneDistance);
    void setHitPosition(const Point3D &_scenePosition);

    int getHitIndex(void) const;
    Vec3D getHitNormal(void) const;
    float getHitDistance(void) const;
    Point3D getHitPosition(void) const;

  private:
    // The index of the object that was hit. Used to retrieve the material of the object.
    int _objectIndex;

    // The normal of the hit point.
    Vec3D _hitNormal;

    // The distance from the ray origin to the hit point.
    float _hitDistance;

    // The position of the hit point.
    Point3D _hitPosition;
};

inline std::ostream &operator<<(std::ostream &os, const RayTracer::HitPayload &hitPayload)
{
    os << "HitPayload: " << std::endl;
    os << "  - Object index: " << hitPayload.getHitIndex() << std::endl;
    os << "  - Hit normal: " << hitPayload.getHitNormal() << std::endl;
    os << "  - Hit distance: " << hitPayload.getHitDistance() << std::endl;
    os << "  - Hit position: " << hitPayload.getHitPosition() << std::endl;
    return os;
}

} // namespace RayTracer

#endif // HIT_PAYLOAD_HPP_
