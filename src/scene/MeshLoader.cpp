/*
** EPITECH PROJECT, 2024
** raytracer
** File description:
** MeshLoader
*/

#include "MeshLoader.hpp"
#include "render/Tangent.hpp"
#include <array>
#include <cctype>
#include <cerrno>
#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <vector>

using namespace RayTracer;

namespace
{
constexpr std::size_t MAX_TRIANGLES = 4000000;
constexpr std::size_t MAX_VERTICES = 8000000;
constexpr std::size_t MAX_TEXTURE_COORDS = 8000000;
constexpr std::size_t MAX_NORMALS = 8000000;
constexpr std::size_t MAX_FACE_VERTICES = 1000000;
constexpr std::size_t MAX_LINE_LENGTH = 1048576;
constexpr float DEGENERATE_EPSILON = 1e-12f;

struct UV
{
    float u = 0.0f;
    float v = 0.0f;
};

struct FaceVertex
{
    std::size_t position = 0;
    std::size_t normal = 0;
    std::size_t texture = 0;
    bool hasNormal = false;
    bool hasTexture = false;
};

[[noreturn]] void fail(const std::string &path, std::size_t line, const std::string &message)
{
    throw std::runtime_error(path + ":" + std::to_string(line) + ": " + message);
}

float checkedFloat(double value, const std::string &path, std::size_t line, const std::string &field)
{
    const float converted = static_cast<float>(value);

    if (!std::isfinite(converted))
        fail(path, line, field + " is outside the supported float range");
    return converted;
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

FaceVertex parseFaceVertex(const std::string &token, std::size_t vertexCount, std::size_t textureCount,
                           std::size_t normalCount, const std::string &path, std::size_t line)
{
    const std::size_t firstSlash = token.find('/');
    const std::string positionToken = firstSlash == std::string::npos ? token : token.substr(0, firstSlash);

    FaceVertex vertex;

    vertex.position = resolveIndex(positionToken, vertexCount, "position", path, line);

    if (firstSlash == std::string::npos)
        return vertex;

    const std::size_t secondSlash = token.find('/', firstSlash + 1);

    if (secondSlash == std::string::npos)
    {
        const std::string textureToken = token.substr(firstSlash + 1);

        if (!textureToken.empty())
        {
            vertex.texture = resolveIndex(textureToken, textureCount, "texture", path, line);
            vertex.hasTexture = true;
        }
        return vertex;
    }

    if (token.find('/', secondSlash + 1) != std::string::npos)
        fail(path, line, "invalid face token '" + token + "'");

    const std::string textureToken = token.substr(firstSlash + 1, secondSlash - firstSlash - 1);
    const std::string normalToken = token.substr(secondSlash + 1);

    if (!textureToken.empty())
    {
        vertex.texture = resolveIndex(textureToken, textureCount, "texture", path, line);
        vertex.hasTexture = true;
    }

    if (normalToken.empty())
        fail(path, line, "empty normal index in face token '" + token + "'");

    vertex.normal = resolveIndex(normalToken, normalCount, "normal", path, line);
    vertex.hasNormal = true;
    return vertex;
}

struct Point2
{
    float x = 0.0f;
    float y = 0.0f;
};

float cross2(const Point2 &a, const Point2 &b, const Point2 &c)
{
    return (b.x - a.x) * (c.y - a.y) - (b.y - a.y) * (c.x - a.x);
}

bool pointInTriangle(const Point2 &point, const Point2 &a, const Point2 &b, const Point2 &c)
{
    constexpr float epsilon = 1e-9f;
    const float d1 = cross2(a, b, point);
    const float d2 = cross2(b, c, point);
    const float d3 = cross2(c, a, point);
    const bool hasNegative = d1 < -epsilon || d2 < -epsilon || d3 < -epsilon;
    const bool hasPositive = d1 > epsilon || d2 > epsilon || d3 > epsilon;

    return !(hasNegative && hasPositive);
}

std::vector<std::array<std::size_t, 3>> fanTriangulate(std::size_t count)
{
    std::vector<std::array<std::size_t, 3>> result;

    if (count < 3)
        return result;
    result.reserve(count - 2);
    for (std::size_t i = 1; i + 1 < count; ++i)
        result.push_back({0, i, i + 1});
    return result;
}

/**
 * Ear-clips a simple polygon. Concave polygons are common in OBJ files; a fan
 * from vertex 0 can emit overlapping triangles.
 */
std::vector<std::array<std::size_t, 3>> triangulateFace(const std::vector<Vec3D> &vertices,
                                                        const std::vector<FaceVertex> &face)
{
    const std::size_t count = face.size();

    if (count <= 3)
        return fanTriangulate(count);

    std::vector<Vec3D> polygon;

    polygon.reserve(count);
    for (const FaceVertex &vertex : face)
        polygon.push_back(vertices[vertex.position]);

    Vec3D normal(0.0f);

    for (std::size_t i = 0; i < count; ++i)
    {
        const Vec3D &a = polygon[i];
        const Vec3D &b = polygon[(i + 1) % count];

        normal.setX(normal.getX() + (a.getY() - b.getY()) * (a.getZ() + b.getZ()));
        normal.setY(normal.getY() + (a.getZ() - b.getZ()) * (a.getX() + b.getX()));
        normal.setZ(normal.getZ() + (a.getX() - b.getX()) * (a.getY() + b.getY()));
    }

    if (!std::isfinite(normal.lengthSquared()) || normal.lengthSquared() < DEGENERATE_EPSILON)
        return fanTriangulate(count);

    int axis = 0;
    float maximum = std::fabs(normal.getX());

    if (std::fabs(normal.getY()) > maximum)
    {
        axis = 1;
        maximum = std::fabs(normal.getY());
    }
    if (std::fabs(normal.getZ()) > maximum)
        axis = 2;

    std::vector<Point2> points;

    points.reserve(count);
    for (const Vec3D &vertex : polygon)
    {
        if (axis == 0)
            points.push_back(Point2{vertex.getY(), vertex.getZ()});
        else if (axis == 1)
            points.push_back(Point2{vertex.getX(), vertex.getZ()});
        else
            points.push_back(Point2{vertex.getX(), vertex.getY()});
    }

    float area2 = 0.0f;

    for (std::size_t i = 0; i < count; ++i)
    {
        const Point2 &a = points[i];
        const Point2 &b = points[(i + 1) % count];

        area2 += a.x * b.y - b.x * a.y;
    }

    if (std::fabs(area2) < 1e-9f)
        return fanTriangulate(count);

    const float orientation = area2 > 0.0f ? 1.0f : -1.0f;
    std::vector<std::size_t> indices(count);
    std::vector<std::array<std::size_t, 3>> triangles;

    for (std::size_t i = 0; i < count; ++i)
        indices[i] = i;
    triangles.reserve(count - 2);

    while (indices.size() > 3)
    {
        const std::size_t size = indices.size();
        bool found = false;

        for (std::size_t i = 0; i < size; ++i)
        {
            const std::size_t previous = indices[(i + size - 1) % size];
            const std::size_t current = indices[i];
            const std::size_t next = indices[(i + 1) % size];

            if (cross2(points[previous], points[current], points[next]) * orientation <= 1e-9f)
                continue;

            bool contains = false;

            for (std::size_t j = 0; j < size; ++j)
            {
                const std::size_t candidate = indices[j];

                if (candidate == previous || candidate == current || candidate == next)
                    continue;
                if (pointInTriangle(points[candidate], points[previous], points[current], points[next]))
                {
                    contains = true;
                    break;
                }
            }
            if (contains)
                continue;

            triangles.push_back({previous, current, next});
            indices.erase(indices.begin() + static_cast<std::ptrdiff_t>(i));
            found = true;
            break;
        }

        if (!found)
        {
            // Non-simple or numerically degenerate polygons fall back to the
            // historical fan so the loader still produces a usable mesh.
            for (std::size_t i = 1; i + 1 < indices.size(); ++i)
                triangles.push_back({indices[0], indices[i], indices[i + 1]});
            return triangles;
        }
    }

    if (indices.size() == 3)
        triangles.push_back({indices[0], indices[1], indices[2]});
    return triangles;
}

bool addTriangle(MeshAsset &asset, const Vec3D &v0, const Vec3D &v1, const Vec3D &v2, const Vec3D &n0,
                 const Vec3D &n1, const Vec3D &n2, const UV &uv0, const UV &uv1, const UV &uv2,
                 int materialGroup)
{
    const Vec3D faceNormal = vec3DCross(v1 - v0, v2 - v0);

    if (!std::isfinite(faceNormal.lengthSquared()) || faceNormal.lengthSquared() < DEGENERATE_EPSILON)
        return false;

    const Vec3D unitNormal = vec3DNormalize(faceNormal);
    const float du1 = uv1.u - uv0.u;
    const float dv1 = uv1.v - uv0.v;
    const float du2 = uv2.u - uv0.u;
    const float dv2 = uv2.v - uv0.v;
    const float determinant = du1 * dv2 - du2 * dv1;
    Vec3D tangent;

    if (std::isfinite(determinant) && std::fabs(determinant) > 1e-9f)
        tangent = orthonormalizeTangent(((v1 - v0) * dv2 - (v2 - v0) * dv1) / determinant, unitNormal);
    else
        tangent = stableTangent(unitNormal);

    asset.triangles.push_back(Triangle{v0, v1, v2, n0, n1, n2, tangent, uv0.u, uv0.v, uv1.u, uv1.v, uv2.u,
                                       uv2.v, materialGroup});
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
    std::vector<UV> textureCoords;
    std::vector<Vec3D> normals;
    std::vector<FaceVertex> face;
    std::unordered_map<std::string, int> materialGroupIds;
    int currentMaterialGroup = -1;
    std::string line;
    std::size_t lineNumber = 0;

    while (std::getline(file, line))
    {
        ++lineNumber;

        if (!line.empty() && line.back() == '\r')
            line.pop_back();

        if (line.size() > MAX_LINE_LENGTH)
            fail(path, lineNumber, "line is too long");

        const std::size_t comment = line.find('#');

        if (comment != std::string::npos)
            line.erase(comment);

        std::istringstream stream(line);
        std::string tag;

        if (!(stream >> tag))
            continue;

        if (tag == "v")
        {
            if (vertices.size() >= MAX_VERTICES)
                throw std::runtime_error(path + ": mesh exceeds the vertex limit");

            double x = 0.0;
            double y = 0.0;
            double z = 0.0;

            if (!(stream >> x >> y >> z) || !std::isfinite(x) || !std::isfinite(y) || !std::isfinite(z))
                fail(path, lineNumber, "invalid vertex");

            vertices.emplace_back(checkedFloat(x, path, lineNumber, "vertex x"),
                                  checkedFloat(y, path, lineNumber, "vertex y"),
                                  checkedFloat(z, path, lineNumber, "vertex z"));
            continue;
        }

        if (tag == "mtllib")
        {
            std::string libraryPath;

            std::getline(stream >> std::ws, libraryPath);

            while (!libraryPath.empty() && std::isspace(static_cast<unsigned char>(libraryPath.back())))
                libraryPath.pop_back();

            if (libraryPath.empty())
                fail(path, lineNumber, "empty mtllib name");

            const std::filesystem::path objectPath(path);
            const std::filesystem::path resolved =
                (objectPath.parent_path() / std::filesystem::path(libraryPath)).lexically_normal();

            asset->materialLibraries.push_back(resolved.string());
            continue;
        }

        if (tag == "usemtl")
        {
            std::string materialName;

            std::getline(stream >> std::ws, materialName);

            while (!materialName.empty() && std::isspace(static_cast<unsigned char>(materialName.back())))
                materialName.pop_back();

            if (materialName.empty())
                fail(path, lineNumber, "empty usemtl name");

            const auto existing = materialGroupIds.find(materialName);

            if (existing != materialGroupIds.end())
                currentMaterialGroup = existing->second;
            else
            {
                currentMaterialGroup = static_cast<int>(asset->materialNames.size());
                asset->materialNames.push_back(materialName);
                materialGroupIds.emplace(materialName, currentMaterialGroup);
            }
            continue;
        }

        if (tag == "vt")
        {
            if (textureCoords.size() >= MAX_TEXTURE_COORDS)
                throw std::runtime_error(path + ": mesh exceeds the texture-coordinate limit");

            double u = 0.0;
            double v = 0.0;

            if (!(stream >> u >> v) || !std::isfinite(u) || !std::isfinite(v))
                fail(path, lineNumber, "invalid texture coordinate");

            textureCoords.push_back(UV{checkedFloat(u, path, lineNumber, "texture u"),
                                       checkedFloat(v, path, lineNumber, "texture v")});
            continue;
        }

        if (tag == "vn")
        {
            if (normals.size() >= MAX_NORMALS)
                throw std::runtime_error(path + ": mesh exceeds the normal limit");

            double x = 0.0;
            double y = 0.0;
            double z = 0.0;

            if (!(stream >> x >> y >> z) || !std::isfinite(x) || !std::isfinite(y) || !std::isfinite(z))
                fail(path, lineNumber, "invalid normal");

            const Vec3D normal(checkedFloat(x, path, lineNumber, "normal x"),
                               checkedFloat(y, path, lineNumber, "normal y"),
                               checkedFloat(z, path, lineNumber, "normal z"));

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
        {
            if (face.size() >= MAX_FACE_VERTICES)
                fail(path, lineNumber, "face has too many vertices");
            face.push_back(
                parseFaceVertex(token, vertices.size(), textureCoords.size(), normals.size(), path, lineNumber));
        }

        if (face.size() < 3)
            fail(path, lineNumber, "face must have at least three vertices");

        const std::vector<std::array<std::size_t, 3>> triangles = triangulateFace(vertices, face);

        for (const std::array<std::size_t, 3> &triangle : triangles)
        {
            if (asset->triangles.size() >= MAX_TRIANGLES)
                throw std::runtime_error(path + ": mesh exceeds the triangle limit");

            const FaceVertex &a = face[triangle[0]];
            const FaceVertex &b = face[triangle[1]];
            const FaceVertex &c = face[triangle[2]];
            const Vec3D &v0 = vertices[a.position];
            const Vec3D &v1 = vertices[b.position];
            const Vec3D &v2 = vertices[c.position];
            const Vec3D faceNormal = vec3DNormalize(vec3DCross(v1 - v0, v2 - v0));

            const Vec3D n0 = a.hasNormal ? normals[a.normal] : faceNormal;
            const Vec3D n1 = b.hasNormal ? normals[b.normal] : faceNormal;
            const Vec3D n2 = c.hasNormal ? normals[c.normal] : faceNormal;
            const UV uv0 = a.hasTexture ? textureCoords[a.texture] : UV{};
            const UV uv1 = b.hasTexture ? textureCoords[b.texture] : UV{};
            const UV uv2 = c.hasTexture ? textureCoords[c.texture] : UV{};

            addTriangle(*asset, v0, v1, v2, n0, n1, n2, uv0, uv1, uv2, currentMaterialGroup);
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
