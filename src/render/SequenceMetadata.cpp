/*
** EPITECH PROJECT, 2024
** raytracer
** File description:
** SequenceMetadata
*/

#include "render/SequenceMetadata.hpp"
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <stdexcept>

using namespace RayTracer;

namespace
{
std::string jsonEscape(const std::string &value)
{
    std::ostringstream escaped;

    for (const unsigned char character : value)
    {
        switch (character)
        {
        case '"':
            escaped << "\\\"";
            break;
        case '\\':
            escaped << "\\\\";
            break;
        case '\b':
            escaped << "\\b";
            break;
        case '\f':
            escaped << "\\f";
            break;
        case '\n':
            escaped << "\\n";
            break;
        case '\r':
            escaped << "\\r";
            break;
        case '\t':
            escaped << "\\t";
            break;
        default:
            if (character < 0x20)
                escaped << "\\u" << std::hex << std::setw(4) << std::setfill('0')
                        << static_cast<int>(character) << std::dec << std::setfill(' ');
            else
                escaped << static_cast<char>(character);
            break;
        }
    }
    return escaped.str();
}

const char *toneMappingName(ToneMapping mapping)
{
    switch (mapping)
    {
    case ToneMapping::NONE:
        return "none";
    case ToneMapping::REINHARD:
        return "reinhard";
    case ToneMapping::ACES:
        return "aces";
    default:
        return "none";
    }
}

double finiteNumber(double value)
{
    return std::isfinite(value) ? value : 0.0;
}

std::string temporaryPath(const std::filesystem::path &destination)
{
    return (destination.parent_path() / (destination.filename().string() + ".tmp")).string();
}
} // namespace

std::string RayTracer::sequenceMetadataPath(const std::string &pattern)
{
    const std::filesystem::path outputPath(pattern.empty() ? "frame-%04d.ppm" : pattern);
    std::string stem = outputPath.stem().string();
    std::size_t conversion = std::string::npos;

    for (std::size_t i = 0; i < stem.size(); ++i)
    {
        if (stem[i] != '%')
            continue;
        if (i + 1 < stem.size() && stem[i + 1] == '%')
        {
            ++i;
            continue;
        }
        conversion = i;
        break;
    }

    if (conversion != std::string::npos)
    {
        std::size_t end = conversion + 1;

        while (end < stem.size() && stem[end] != 'd' && stem[end] != 'i' && stem[end] != 'u')
            ++end;
        if (end < stem.size())
            stem.erase(conversion, end - conversion + 1);
    }

    while (!stem.empty() && (stem.back() == '-' || stem.back() == '_' || stem.back() == '.'))
        stem.pop_back();
    if (stem.empty())
        stem = "sequence";

    return (outputPath.parent_path() / (stem + ".json")).string();
}

FrameMetadata RayTracer::makeFrameMetadata(int frame, double time, const std::string &path, uint32_t seed,
                                           const Camera &camera, bool rendered)
{
    FrameMetadata metadata;

    metadata.frame = frame;
    metadata.time = time;
    metadata.path = path;
    metadata.seed = seed;
    metadata.position = camera.getLookFrom();
    metadata.lookAt = camera.getLookAt();
    metadata.fieldOfView = camera.getVerticalFovDegree();
    metadata.lensRadius = camera.getLensRadius();
    metadata.focusDistance = camera.getFocusDistance();
    metadata.rendered = rendered;
    return metadata;
}

void RayTracer::writeSequenceMetadata(const std::string &path, const SequenceMetadata &metadata)
{
    if (path.empty())
        throw std::invalid_argument("Metadata path must not be empty");

    std::ostringstream json;

    json << std::setprecision(9);
    json << "{\n";
    json << "  \"schema\": 1,\n";
    json << "  \"scene\": \"" << jsonEscape(metadata.scenePath) << "\",\n";
    json << "  \"renderer\": \"" << jsonEscape(metadata.renderer) << "\",\n";
    json << "  \"width\": " << metadata.width << ",\n";
    json << "  \"height\": " << metadata.height << ",\n";
    json << "  \"samples\": " << metadata.samples << ",\n";
    json << "  \"seed\": " << metadata.seed << ",\n";
    json << "  \"fps\": " << finiteNumber(metadata.fps) << ",\n";
    json << "  \"startFrame\": " << metadata.startFrame << ",\n";
    json << "  \"endFrame\": " << metadata.endFrame << ",\n";
    json << "  \"shutter\": {\"angle\": " << finiteNumber(metadata.shutterAngle)
         << ", \"samples\": " << metadata.shutterSamples << "},\n";
    json << "  \"postProcess\": {\"exposure\": " << finiteNumber(metadata.postProcess.exposure)
         << ", \"toneMapping\": \"" << toneMappingName(metadata.postProcess.toneMapping) << "\""
         << ", \"srgb\": " << (metadata.postProcess.srgb ? "true" : "false")
         << ", \"bloom\": " << (metadata.postProcess.bloom ? "true" : "false")
         << ", \"bloomThreshold\": " << finiteNumber(metadata.postProcess.bloomThreshold)
         << ", \"bloomIntensity\": " << finiteNumber(metadata.postProcess.bloomIntensity)
         << ", \"bloomRadius\": " << metadata.postProcess.bloomRadius << "},\n";
    json << "  \"frames\": [\n";

    for (std::size_t i = 0; i < metadata.frames.size(); ++i)
    {
        const FrameMetadata &frame = metadata.frames[i];
        const Vec3D &position = frame.position;
        const Vec3D &lookAt = frame.lookAt;

        json << "    {\"frame\": " << frame.frame << ", \"time\": " << finiteNumber(frame.time)
             << ", \"file\": \"" << jsonEscape(frame.path) << "\""
             << ", \"seed\": " << frame.seed
             << ", \"rendered\": " << (frame.rendered ? "true" : "false")
             << ", \"camera\": {\"position\": [" << finiteNumber(position.getX()) << ", "
             << finiteNumber(position.getY()) << ", " << finiteNumber(position.getZ()) << "]"
             << ", \"lookAt\": [" << finiteNumber(lookAt.getX()) << ", " << finiteNumber(lookAt.getY()) << ", "
             << finiteNumber(lookAt.getZ()) << "]"
             << ", \"fieldOfView\": " << finiteNumber(frame.fieldOfView)
             << ", \"lensRadius\": " << finiteNumber(frame.lensRadius)
             << ", \"focusDistance\": " << finiteNumber(frame.focusDistance) << "}}";

        if (i + 1 < metadata.frames.size())
            json << ",";
        json << "\n";
    }

    json << "  ]\n";
    json << "}\n";

    const std::filesystem::path destination(path);
    const std::string temporary = temporaryPath(destination);
    std::ofstream file(temporary, std::ios::out | std::ios::binary | std::ios::trunc);

    if (!file.is_open())
        throw std::runtime_error("Cannot open metadata file: " + temporary);

    file << json.str();

    if (!file.good())
        throw std::runtime_error("Failed to write metadata file: " + temporary);
    file.close();

    std::error_code error;

    std::filesystem::rename(temporary, destination, error);
    if (error)
    {
        std::filesystem::remove(temporary, error);
        throw std::runtime_error("Failed to finalize metadata file: " + path);
    }
}
