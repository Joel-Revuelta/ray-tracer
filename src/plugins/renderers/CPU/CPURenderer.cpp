/*
** EPITECH PROJECT, 2024
** raytracer
** File description:
** CPURenderer
*/

#include "plugins/renderers/CPU/CPURenderer.hpp"
#include "plugins/renderers/CPU/Renderer.hpp"
#include "render/ImageWriter.hpp"
#include "render/PostProcess.hpp"
#include "render/SequenceMetadata.hpp"
#include "render/VideoWriter.hpp"
#include <algorithm>
#include <chrono>
#include <cmath>
#include <iomanip>
#include <iostream>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <utility>

using namespace RayTracer;

namespace
{
std::string formatDuration(double seconds)
{
    if (!std::isfinite(seconds) || seconds < 0.0)
        seconds = 0.0;

    const int total = static_cast<int>(seconds);
    std::ostringstream formatted;

    formatted << std::setfill('0') << std::setw(2) << total / 3600 << ":" << std::setw(2) << (total / 60) % 60
              << ":" << std::setw(2) << total % 60;
    return formatted.str();
}
} // namespace

std::string CPURenderer::getName(void) const
{
    return "cpu";
}

std::string CPURenderer::getDescription(void) const
{
    return "Multithreaded CPU renderer";
}

bool CPURenderer::isAvailable(std::string &reason) const
{
    (void)reason;
    return true;
}

void CPURenderer::run(const RenderContext &context)
{
    if (!context.scene || !context.camera)
        throw std::invalid_argument("CPU renderer requires a scene and camera");
    if (context.options.frames <= 0)
        throw std::invalid_argument("CPU renderer requires at least one sample");

    if (context.options.animation)
    {
        if (!context.animation)
            throw std::invalid_argument("CPU animation requires a camera animation");
        if (context.gui)
            throw std::invalid_argument("Animated rendering requires --cli");

        const int startFrame = context.options.startFrame;
        const int endFrame = context.options.endFrame;

        if (startFrame < 0 || endFrame < startFrame)
            throw std::invalid_argument("Invalid animation frame range");

        const double fps = context.options.animationFps;
        const ShutterSettings shutter{context.options.shutterAngle, context.options.shutterSamples};
        const bool motionBlur = shutter.angle > 0.0f && shutter.samples > 1;
        const int subframes = motionBlur ? shutter.samples : 1;
        const int totalFrames = endFrame - startFrame + 1;
        const int width = context.options.width;
        const int height = context.options.height;
        const std::size_t pixelCount = static_cast<std::size_t>(width) * static_cast<std::size_t>(height);
        const bool writeFrames = !context.options.outputPath.empty();
        const std::string pattern = writeFrames ? context.options.outputPath : std::string();
        const bool writeVideo = !context.options.videoPath.empty();

        if (!writeFrames && !writeVideo)
            throw std::invalid_argument("CPU animation requires a frame pattern or a video path");
        if (writeVideo && context.options.resume)
            throw std::invalid_argument("CPU video output cannot be resumed");

        std::unique_ptr<VideoWriter> video;

        if (writeVideo)
        {
            const VideoSettings videoSettings{context.options.videoPath, context.options.videoCodec,
                                              context.options.videoCrf,   context.options.videoPreset,
                                              context.options.videoBitrate, fps, width, height};

            video = std::make_unique<VideoWriter>(videoSettings);
        }

        const auto sequenceStart = std::chrono::steady_clock::now();
        int completedFrames = 0;
        int renderedFrames = 0;

        for (int frame = startFrame; frame <= endFrame; ++frame)
        {
            const std::string path = writeFrames ? formatFramePath(pattern, frame) : std::string();
            const double frameTime = context.animation->timeAtFrame(frame, fps);

            if (writeFrames && context.options.resume && frameFileExists(path))
            {
                ++completedFrames;
                std::clog << "Skipped existing animation frame " << frame << " -> " << path << std::endl;
                continue;
            }

            if (context.sceneAnimation)
            {
                const bool instancesChanged = context.sceneAnimation->apply(*context.scene, frameTime);

                if (instancesChanged)
                    context.scene->buildAcceleration();
            }

            std::vector<Color3D> frameHDR(pixelCount, Color3D(0.0f));
            const float subframeWeight = 1.0f / static_cast<float>(subframes);

            for (int subframe = 0; subframe < subframes; ++subframe)
            {
                std::shared_ptr<Camera> camera = context.animation->cameraAt(
                    shutterSampleTime(frameTime, fps, shutter, subframe));

                camera->initialize(width, height, camera->getLookAt(), camera->getLookFrom(),
                                   camera->getVerticalFovDegree());

                if (context.options.lensRadiusOverride >= 0.0f)
                    camera->setLensRadius(context.options.lensRadiusOverride);
                if (context.options.focusDistanceOverride > 0.0f)
                    camera->setFocusDistance(context.options.focusDistanceOverride);

                uint32_t seed = context.options.seed + static_cast<uint32_t>(frame) * 0x9e3779b9u;

                if (subframes > 1)
                    seed += static_cast<uint32_t>(subframe) * 0x85ebca6bu;

                Renderer renderer(context.options.frames, context.scene, camera, nullptr, "",
                                  context.options.postProcess,
                                  static_cast<std::size_t>(std::max(context.options.threads, 0)), seed);
                const std::vector<Color3D> hdr = renderer.renderHDR();

                if (hdr.size() != pixelCount)
                    throw std::runtime_error("CPU animation returned an unexpected HDR image size");

                for (std::size_t i = 0; i < pixelCount; ++i)
                    frameHDR[i] += hdr[i] * subframeWeight;
            }

            const std::vector<uint32_t> pixels =
                postProcessToRGBA(std::move(frameHDR), width, height, context.options.postProcess);

            if (writeFrames)
                writeImageFileAtomic(path, width, height, pixels);
            if (video)
                video->writeFrame(pixels);
            ++completedFrames;
            ++renderedFrames;

            const double elapsed = std::chrono::duration<double>(std::chrono::steady_clock::now() - sequenceStart).count();
            const double average = completedFrames > 0 ? elapsed / completedFrames : 0.0;
            const double eta = average * (totalFrames - completedFrames);
            const std::string &target = writeFrames ? path : context.options.videoPath;

            std::clog << "[" << completedFrames << "/" << totalFrames << "] frame " << frame << " -> " << target
                      << " (elapsed " << formatDuration(elapsed) << ", ETA " << formatDuration(eta) << ")"
                      << std::endl;
        }

        if (video)
        {
            video->finish();
            std::clog << "Wrote video -> " << context.options.videoPath << " (" << context.options.videoCodec
                      << ", " << width << "x" << height << ", " << fps << " fps)" << std::endl;
        }

        if (context.options.resume && renderedFrames == 0)
        {
            std::clog << "All animation frames already exist; leaving metadata unchanged." << std::endl;
            return;
        }

        SequenceMetadata metadata;

        metadata.scenePath = context.options.scenePath;
        metadata.renderer = context.options.rendererName;
        metadata.width = width;
        metadata.height = height;
        metadata.samples = context.options.frames;
        metadata.seed = context.options.seed;
        metadata.fps = fps;
        metadata.startFrame = startFrame;
        metadata.endFrame = endFrame;
        metadata.shutterAngle = shutter.angle;
        metadata.shutterSamples = shutter.samples;
        metadata.postProcess = context.options.postProcess;
        metadata.video.path = context.options.videoPath;
        metadata.video.codec = context.options.videoCodec;
        metadata.video.crf = context.options.videoCrf;
        metadata.video.preset = context.options.videoPreset;
        metadata.video.bitrate = context.options.videoBitrate;
        metadata.video.fps = fps;
        metadata.video.enabled = writeVideo;

        if (context.sceneAnimation)
        {
            for (const InstanceTrack &track : context.sceneAnimation->getInstanceTracks())
                metadata.sceneAnimation.instances.push_back(track.target);
            for (const LightTrack &track : context.sceneAnimation->getLightTracks())
                metadata.sceneAnimation.lights.push_back(track.target);
        }

        for (int frame = startFrame; frame <= endFrame; ++frame)
        {
            const double frameTime = context.animation->timeAtFrame(frame, fps);
            std::shared_ptr<Camera> camera = context.animation->cameraAt(frameTime);

            if (context.options.lensRadiusOverride >= 0.0f)
                camera->setLensRadius(context.options.lensRadiusOverride);
            if (context.options.focusDistanceOverride > 0.0f)
                camera->setFocusDistance(context.options.focusDistanceOverride);

            const std::string path = writeFrames ? formatFramePath(pattern, frame) : std::string();
            const uint32_t seed = context.options.seed + static_cast<uint32_t>(frame) * 0x9e3779b9u;
            const bool rendered = writeFrames ? frameFileExists(path) : true;

            metadata.frames.push_back(makeFrameMetadata(frame, frameTime, path, seed, *camera, rendered));
        }

        const std::string metadataPath =
            writeFrames ? sequenceMetadataPath(pattern) : sequenceMetadataPath(context.options.videoPath);

        writeSequenceMetadata(metadataPath, metadata);
        std::clog << "Wrote sequence metadata -> " << metadataPath << std::endl;
        return;
    }

    if (context.camera->getImageWidth() != context.options.width ||
        context.camera->getImageHeight() != context.options.height)
    {
        context.camera->initialize(context.options.width, context.options.height, context.camera->getLookAt(),
                                   context.camera->getLookFrom(), context.camera->getVerticalFovDegree());
    }

    Renderer renderer(context.options.frames, context.scene, context.camera, context.gui, context.options.outputPath,
                      context.options.postProcess, static_cast<std::size_t>(std::max(context.options.threads, 0)),
                      context.options.seed);
    renderer.run();
}

extern "C" std::shared_ptr<RayTracer::IRenderer> create(void)
{
    return std::make_shared<RayTracer::CPURenderer>();
}
