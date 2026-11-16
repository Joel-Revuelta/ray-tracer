#include "Camera.hpp"
#include "Vec3D.hpp"
#include "render/SamplingMath.hpp"
#include <iostream>

int main(void)
{
    const RayTracer::Vec3D vector(1.0f, 2.0f, 3.0f);
    const RayTracer::Camera camera(RayTracer::Vec3D(0.0f), RayTracer::Vec3D(0.0f, 0.0f, -5.0f), 60.0f);

    std::cout << vector.length() << " " << camera.getImageWidth() << " "
              << RayTracer::smithGGXSampling(1.0f, 1.0f) << "\n";
    return 0;
}
