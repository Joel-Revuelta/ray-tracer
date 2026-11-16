/*
** EPITECH PROJECT, 2024
** raytracer
** File description:
** ParserInternal.cpp
*/

#include "ParserInternal.hpp"
#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>
#include <string>

namespace RayTracer
{
namespace ParserInternal
{

void requireFinite(double value, const std::string &name)
{
    // The renderer stores geometry in float, so a finite double that overflows
    // float is still invalid input.
    if (!std::isfinite(value) || !std::isfinite(static_cast<float>(value)))
        throw std::runtime_error(name + " must be finite and representable as a float");
}

void requirePositive(double value, const std::string &name)
{
    requireFinite(value, name);
    if (value <= 0.0)
        throw std::runtime_error(name + " must be greater than zero");
}

void requireNonNegative(double value, const std::string &name)
{
    requireFinite(value, name);
    if (value < 0.0)
        throw std::runtime_error(name + " must not be negative");
}

double settingAsDouble(const libconfig::Setting &setting, const std::string &name)
{
    if (!setting.isNumber())
        throw std::runtime_error(name + " must be a number");

    if (setting.getType() == libconfig::Setting::TypeFloat)
        return static_cast<double>(setting);
    if (setting.getType() == libconfig::Setting::TypeInt64)
        return static_cast<double>(static_cast<long long>(setting));
    return static_cast<double>(static_cast<int>(setting));
}

int settingAsInt(const libconfig::Setting &setting, const std::string &name, int minimum, int maximum)
{
    const double value = settingAsDouble(setting, name);

    requireFinite(value, name);
    if (value != std::floor(value) || value < static_cast<double>(minimum) ||
        value > static_cast<double>(maximum))
        throw std::runtime_error(name + " must be an integer between " + std::to_string(minimum) + " and " +
                                 std::to_string(maximum));
    return static_cast<int>(value);
}

uint32_t settingAsUInt32(const libconfig::Setting &setting, const std::string &name)
{
    const double value = settingAsDouble(setting, name);

    requireFinite(value, name);
    if (value != std::floor(value) || value < 0.0 ||
        value > static_cast<double>(std::numeric_limits<uint32_t>::max()))
        throw std::runtime_error(name + " must be an unsigned 32-bit integer");
    return static_cast<uint32_t>(value);
}

void rejectUnknownKeys(const libconfig::Setting &setting, const std::string &context,
                       std::initializer_list<const char *> allowed)
{
    if (!setting.isGroup())
        return;

    for (int i = 0; i < setting.getLength(); ++i)
    {
        const std::string name = setting[i].getName();

        if (name.empty())
            continue;

        bool known = false;

        for (const char *candidate : allowed)
        {
            if (name == candidate)
            {
                known = true;
                break;
            }
        }
        if (!known)
            throw std::runtime_error(context + ": unknown setting '" + name + "'");
    }
}

void requireNonZeroVector(const Vec3D &vector, const std::string &name)
{
    if (!std::isfinite(vector.lengthSquared()) || vector.lengthSquared() < 1e-12f)
        throw std::runtime_error(name + " must be a non-zero vector");
}

void requireRange(double value, double minimum, double maximum, const std::string &name)
{
    requireFinite(value, name);
    if (value < minimum || value > maximum)
        throw std::runtime_error(name + " must be between " + std::to_string(minimum) + " and " +
                                 std::to_string(maximum));
}

float luminance(const Color3D &color)
{
    return 0.2126f * color.getX() + 0.7152f * color.getY() + 0.0722f * color.getZ();
}

float roughnessFromShininess(float shininess)
{
    const float roughness = std::sqrt(2.0f / (std::max(shininess, 0.0f) + 2.0f));

    return std::clamp(roughness, 0.02f, 1.0f);
}

uint parseMaterialIndex(const libconfig::Setting &setting)
{
    const int index = settingAsInt(setting.lookup("material"), "Material index", 0,
                                   static_cast<int>(std::numeric_limits<int>::max()));

    return static_cast<uint>(index);
}

} // namespace ParserInternal
} // namespace RayTracer
