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

struct FaceVertex
{
    std::size_t position = 0;
    std::size_t normal = 0;
    bool hasNormal = false;
};

[[noreturn]] void fail(const std::string &path, std::size_t line, const std::string &message)
{
    throw std::runtime_error(path + ":" + std::to_string(line) + ": " + message);
}

std::size_t resolveIndex(const std::string &token, std::size_t count, const std::string &kind,
                         const std::string &path, std::size_t line)
{
    if (token.empty())
        fail(path, line, "empty " + kind + " index");

    errno = 0;
    char *end = nullptr;
    const long parsed = std::strtol(token.c_str(), &end, 10);

    if (errno != 0 || end == token.c_str() || *end != '\0' || parsed == 0)
        fail(path, line, "invalid " + kind + " index '" + token + "'");

    const long resolved = parsed < 0 ? static_cast<long>(count) + parsed : parsed - 1;

    if (resolved < 0 || resolved >= static_cast<long>(count))
        fail(path, line, kind + " index out of range '" + token + "'");

    return static_cast<std::size_t>(resolved);
}

FaceVertex parseFaceVertex(const std::string &token, std::size_t vertexCount, std::size_t normalCount,
                           const std::string &path, std::size_t line)
{
    const std::size_t firstSlash = token.find('/');
    const std::string positionToken = firstSlash == std::string::npos ? token : token.substr(0, firstSlash);

    FaceVertex vertex;

    vertex.position = resolveIndex(positionToken, vertexCount, "position", path, line);

    if (firstSlash == std::string::npos)
        return vertex;

    const std::size_t secondSlash = token.find('/', firstSlash + 1);

    if (secondSlash == std::string::npos)
        return vertex;

    if (token.find('/', secondSlash + 1) != std::string::npos)
        fail(path, line, "invalid face token '" + token + "'");

    const std::string normalToken = token.substr(secondSlash + 1);

    if (normalToken.empty())
        fail(path, line, "empty normal index in face token '" + token + "'");

    vertex.normal = resolveIndex(normalToken, normalCount, "normal", path, line);
    vertex.hasNormal = true;
    return vertex;
}

bool addTriangle(MeshAsset &asset, const Vec3D &v0, const Vec3D &v1, const Vec3D &v2, const Vec3D &n0,
                 const Vec3D &n1, const Vec3D &n2)
{
    const Vec3D faceNormal = vec3DCross(v1 - v0, v2 - v0);

    if (!std::isfinite(faceNormal.lengthSquared()) || faceNormal.lengthSquared() < DEGENERATE_EPSILON)
        return false;

    asset.triangles.push_back(Triangle{v0, v1, v2, n0, n1, n2});
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
    std::vector<Vec3D> normals;
    std::vector<FaceVertex> face;
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

        if (tag == "vn")
        {
            double x = 0.0;
            double y = 0.0;
            double z = 0.0;

            if (!(stream >> x >> y >> z) || !std::isfinite(x) || !std::isfinite(y) || !std::isfinite(z))
                fail(path, lineNumber, "invalid normal");

            const Vec3D normal(static_cast<float>(x), static_cast<float>(y), static_cast<float>(z));

            if (!std::isfinite(normal.lengthSquared()) || normal.lengthSquared() < DEGENERATE_EPSILON)
                fail(path, lineNumber, "normal must be non-zero");

            normals.push_back(vec3DNormalize(normal));
            continue;
        }

        if (tag != "f")
            continue;

        face.clear();

        std::string token;

        while (stream >> token)
            face.push_back(parseFaceVertex(token, vertices.size(), normals.size(), path, lineNumber));

        if (face.size() < 3)
            fail(path, lineNumber, "face must have at least three vertices");

        for (std::size_t i = 1; i + 1 < face.size(); ++i)
        {
            if (asset->triangles.size() >= MAX_TRIANGLES)
                throw std::runtime_error(path + ": mesh exceeds the triangle limit");

            const FaceVertex &a = face[0];
            const FaceVertex &b = face[i];
            const FaceVertex &c = face[i + 1];
            const Vec3D &v0 = vertices[a.position];
            const Vec3D &v1 = vertices[b.position];
            const Vec3D &v2 = vertices[c.position];
            const Vec3D faceNormal = vec3DNormalize(vec3DCross(v1 - v0, v2 - v0));

            const Vec3D n0 = a.hasNormal ? normals[a.normal] : faceNormal;
            const Vec3D n1 = b.hasNormal ? normals[b.normal] : faceNormal;
            const Vec3D n2 = c.hasNormal ? normals[c.normal] : faceNormal;

            addTriangle(*asset, v0, v1, v2, n0, n1, n2);
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
