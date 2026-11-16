/*
** EPITECH PROJECT, 2024
** raytracer
** File description:
** ParserInternal.hpp
*/

#ifndef PARSER_INTERNAL_HPP_
#define PARSER_INTERNAL_HPP_

#include "Color.hpp"
#include "Vec3D.hpp"
#include <libconfig.h++>
#include <cstddef>
#include <cstdint>
#include <initializer_list>
#include <string>
#include <sys/types.h>
#include <unordered_map>

namespace RayTracer
{
namespace ParserInternal
{

inline constexpr std::size_t MAX_ANIMATION_KEYFRAMES = 100000;
inline constexpr std::size_t MAX_ANIMATION_TRACKS = 10000;

void requireFinite(double value, const std::string &name);
void requirePositive(double value, const std::string &name);
void requireNonNegative(double value, const std::string &name);
double settingAsDouble(const libconfig::Setting &setting, const std::string &name);
int settingAsInt(const libconfig::Setting &setting, const std::string &name, int minimum, int maximum);
uint32_t settingAsUInt32(const libconfig::Setting &setting, const std::string &name);
void rejectUnknownKeys(const libconfig::Setting &setting, const std::string &context,
                       std::initializer_list<const char *> allowed);
void requireNonZeroVector(const Vec3D &vector, const std::string &name);
void requireRange(double value, double minimum, double maximum, const std::string &name);
float luminance(const Color3D &color);
float roughnessFromShininess(float shininess);
uint parseMaterialIndex(const libconfig::Setting &setting);

class NestingGuard
{
  public:
    explicit NestingGuard(int &depth) : _depth(depth)
    {
        ++_depth;
    }

    ~NestingGuard(void)
    {
        --_depth;
    }

    NestingGuard(const NestingGuard &) = delete;
    NestingGuard &operator=(const NestingGuard &) = delete;

  private:
    int &_depth;
};

class MaterialCountGuard
{
  public:
    explicit MaterialCountGuard(uint &count) : _count(count), _saved(count)
    {
        _count = 0;
    }

    ~MaterialCountGuard(void)
    {
        _count = _saved;
    }

    MaterialCountGuard(const MaterialCountGuard &) = delete;
    MaterialCountGuard &operator=(const MaterialCountGuard &) = delete;

  private:
    uint &_count;
    uint _saved;
};

class MtlIndexGuard
{
  public:
    explicit MtlIndexGuard(std::unordered_map<std::string, uint> &indices) : _indices(indices), _saved(indices)
    {
        _indices.clear();
    }

    ~MtlIndexGuard(void)
    {
        _indices = _saved;
    }

    MtlIndexGuard(const MtlIndexGuard &) = delete;
    MtlIndexGuard &operator=(const MtlIndexGuard &) = delete;

  private:
    std::unordered_map<std::string, uint> &_indices;
    std::unordered_map<std::string, uint> _saved;
};

class PrefabFlagGuard
{
  public:
    explicit PrefabFlagGuard(bool &flag) : _flag(flag), _saved(flag)
    {
        _flag = true;
    }

    ~PrefabFlagGuard(void)
    {
        _flag = _saved;
    }

    PrefabFlagGuard(const PrefabFlagGuard &) = delete;
    PrefabFlagGuard &operator=(const PrefabFlagGuard &) = delete;

  private:
    bool &_flag;
    bool _saved;
};

} // namespace ParserInternal
} // namespace RayTracer

#endif // PARSER_INTERNAL_HPP_
