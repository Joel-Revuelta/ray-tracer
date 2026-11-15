/*
** EPITECH PROJECT, 2024
** raytracer
** File description:
** Environment
*/

#ifndef ENVIRONMENT_HPP_
#define ENVIRONMENT_HPP_

#include "Color.hpp"
#include "Vec3D.hpp"
#include <cstdint>
#include <memory>
#include <vector>

namespace RayTracer
{

class Texture;

/**
 * Importance-sampling distribution over the texels of an equirectangular
 * environment map. Weights include the sin(theta) solid-angle term so rows near
 * the poles do not receive disproportionate samples.
 */
struct EnvironmentDistribution
{
    int width = 0;
    int height = 0;
    // Normalized CDF over u for each v row.
    std::vector<float> conditionalCdf;
    // Normalized CDF over v.
    std::vector<float> marginalCdf;
    // Unnormalized sum of luminance * sin(theta).
    float total = 0.0f;
};

struct EnvironmentSample
{
    Vec3D direction;
    Color3D radiance;
    float pdf = 0.0f;
    bool valid = false;
};

float environmentLuminance(const Color3D &color);
std::shared_ptr<const EnvironmentDistribution> buildEnvironmentDistribution(const Texture &texture);
EnvironmentSample sampleEnvironment(const Texture &texture, const EnvironmentDistribution &distribution,
                                     uint32_t &seed);
float environmentPdf(const Texture &texture, const EnvironmentDistribution &distribution, const Vec3D &direction);

} // namespace RayTracer

#endif // ENVIRONMENT_HPP_
