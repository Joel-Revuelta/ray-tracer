/*
** EPITECH PROJECT, 2024
** raytracer
** File description:
** MeshLoader
*/

#include "MeshLoader.hpp"
#include <cerrno>
#include <cmath>
#include <cstdlib>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

using namespace RayTracer;

namespace
{
constexpr std::size_t MAX_TRIANGLES = 4000000;
constexpr float DEGENERATE_EPSILON = 1e-12f;

[[noreturn]] void fail(const std::string &path, std::size_t line, const std::string &message)
{
    throw std::runtime_error(path + ":" + std::to_string(line) + ": " + message);
}

std::size_t parseFaceIndex(const std::string &token, std::size_t vertexCount, const std::string &path,
                           std::size_t line)
{
    const std::size_t slash = token.find('/');
    const std::string vertexToken = slash == std::string::npos ? token : token.substr(0, slash);

    if (vertexToken.empty())
        fail(path, line, "empty face index");

    errno = 0;
    char *end = nullptr;
    const long parsed = std::strtol(vertexToken.c_str(), &end, 10);

    if (errno != 0 || end == vertexToken.c_str() || *end != '\0' || parsed == 0)
        fail(path, line, "invalid face index '" + token + "'");

    const long resolved = parsed < 0 ? static_cast<long>(vertexCount) + parsed : parsed - 1;

    if (resolved < 0 || resolved >= static_cast<long>(vertexCount))
        fail(path, line, "face index out of range '" + token + "'");

    return static_cast<std::size_t>(resolved);
}

bool addTriangle(MeshAsset &asset, const Vec3D &v0, const Vec3D &v1, const Vec3D &v2)
{
    const Vec3D normal = vec3DCross(v1 - v0, v2 - v0);

    if (!std::isfinite(normal.lengthSquared()) || normal.lengthSquared() < DEGENERATE_EPSILON)
        return false;

    asset.triangles.push_back(Triangle{v0, v1, v2});
    return true;
}
} // namespace

std::shared_ptr<const MeshAsset> RayTracer::loadObjMesh(const std::string &path)
{
    std::ifstream file(path);

    if (!file)
        throw std::runtime_error("Cannot open mesh file: " + path);

    auto asset = std::make_shared<MeshAsset>();
    std::vector<Vec3D> vertices;
    std::vector<std::size_t> faceIndices;
    std::string line;
    std::size_t lineNumber = 0;

    while (std::getline(file, line))
    {
        ++lineNumber;

        if (!line.empty() && line.back() == '\r')
            line.pop_back();

        const std::size_t comment = line.find('#');

        if (comment != std::string::npos)
            line.erase(comment);

        std::istringstream stream(line);
        std::string tag;

        if (!(stream >> tag))
            continue;

        if (tag == "v")
        {
            double x = 0.0;
            double y = 0.0;
            double z = 0.0;

            if (!(stream >> x >> y >> z) || !std::isfinite(x) || !std::isfinite(y) || !std::isfinite(z))
                fail(path, lineNumber, "invalid vertex");

            vertices.emplace_back(static_cast<float>(x), static_cast<float>(y), static_cast<float>(z));
            continue;
        }

        if (tag != "f")
            continue;

        faceIndices.clear();

        std::string token;

        while (stream >> token)
            faceIndices.push_back(parseFaceIndex(token, vertices.size(), path, lineNumber));

        if (faceIndices.size() < 3)
            fail(path, lineNumber, "face must have at least three vertices");

        for (std::size_t i = 1; i + 1 < faceIndices.size(); ++i)
        {
            if (asset->triangles.size() >= MAX_TRIANGLES)
                throw std::runtime_error(path + ": mesh exceeds the triangle limit");

            addTriangle(*asset, vertices[faceIndices[0]], vertices[faceIndices[i]], vertices[faceIndices[i + 1]]);
        }
    }

    if (asset->triangles.empty())
        throw std::runtime_error(path + ": mesh contains no triangles");

    std::vector<BVHItem> items;

    items.reserve(asset->triangles.size());

    for (std::size_t i = 0; i < asset->triangles.size(); ++i)
    {
        const Triangle &triangle = asset->triangles[i];
        AABB bounds;

        bounds.expand(triangle.v0);
        bounds.expand(triangle.v1);
        bounds.expand(triangle.v2);
        asset->bounds.expand(bounds);
        items.push_back(BVHItem{static_cast<std::uint32_t>(i), bounds, bounds.centroid()});
    }

    asset->bvh = buildBVHFromItems(std::move(items), "triangles");
    return asset;
}
