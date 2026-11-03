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
    void setShadingNormal(const Vec3D &shadingNormal);
    void setHitDistance(const float &_sceneDistance);
    void setHitPosition(const Point3D &_scenePosition);

    int getHitIndex(void) const;
    Vec3D getHitNormal(void) const;
    Vec3D getShadingNormal(void) const;
    float getHitDistance(void) const;
    Point3D getHitPosition(void) const;

  private:
    // The index of the object that was hit. Used to retrieve the material of the object.
    int _objectIndex;

    // The geometric normal of the hit point, used for ray offsets and
    // dielectric behavior.
    Vec3D _hitNormal;

    // The normal used for shading. Defaults to the geometric normal, but
    // smooth meshes can provide an interpolated normal.
    Vec3D _shadingNormal;

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
