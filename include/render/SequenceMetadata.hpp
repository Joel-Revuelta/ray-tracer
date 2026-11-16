/*
** EPITECH PROJECT, 2024
** raytracer
** File description:
** SequenceMetadata
*/

#ifndef SEQUENCE_METADATA_HPP_
#define SEQUENCE_METADATA_HPP_

#include "Camera.hpp"
#include "render/PostProcess.hpp"
#include <cstdint>
#include <string>
#include <vector>

namespace RayTracer
{

struct FrameMetadata
{
    int frame = 0;
    double time = 0.0;
    std::string path;
    uint32_t seed = 0;
    Vec3D position;
    Vec3D lookAt;
    float fieldOfView = 60.0f;
    float lensRadius = 0.0f;
    float focusDistance = 0.0f;
    bool rendered = false;
};

struct SequenceMetadata
{
    std::string scenePath;
    std::string renderer;
    int width = 0;
    int height = 0;
    int samples = 0;
    uint32_t seed = 0;
    double fps = 24.0;
    int startFrame = 0;
    int endFrame = 0;
    float shutterAngle = 0.0f;
    int shutterSamples = 1;
    PostProcessSettings postProcess;
    std::vector<FrameMetadata> frames;
};

/** Returns the JSON sidecar path for an animation output pattern. */
std::string sequenceMetadataPath(const std::string &pattern);

/** Builds the metadata record for one frame from its interpolated camera. */
FrameMetadata makeFrameMetadata(int frame, double time, const std::string &path, uint32_t seed,
                                const Camera &camera, bool rendered);

/** Writes the sequence metadata atomically as JSON. */
void writeSequenceMetadata(const std::string &path, const SequenceMetadata &metadata);

} // namespace RayTracer

#endif // SEQUENCE_METADATA_HPP_
