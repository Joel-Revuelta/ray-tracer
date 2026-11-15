/*
** EPITECH PROJECT, 2024
** raytracer
** File description:
** CPURenderer
*/

#include "plugins/renderers/CPU/CPURenderer.hpp"
#include "plugins/renderers/CPU/Renderer.hpp"
#include "render/ImageWriter.hpp"
#include <algorithm>
#include <iostream>
#include <stdexcept>

using namespace RayTracer;

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

        for (int frame = startFrame; frame <= endFrame; ++frame)
        {
            std::shared_ptr<Camera> camera =
                context.animation->cameraAt(context.animation->timeAtFrame(frame, context.options.animationFps));

            camera->initialize(context.options.width, context.options.height, camera->getLookAt(),
                               camera->getLookFrom(), camera->getVerticalFovDegree());

            if (context.options.lensRadiusOverride >= 0.0f)
                camera->setLensRadius(context.options.lensRadiusOverride);
            if (context.options.focusDistanceOverride > 0.0f)
                camera->setFocusDistance(context.options.focusDistanceOverride);

            const std::string path = formatFramePath(context.options.outputPath, frame);
            const uint32_t seed =
                context.options.seed + static_cast<uint32_t>(frame) * 0x9e3779b9u;

            Renderer renderer(context.options.frames, context.scene, camera, nullptr, path,
                              context.options.postProcess,
                              static_cast<std::size_t>(std::max(context.options.threads, 0)), seed);
            renderer.run();
            std::clog << "Rendered animation frame " << frame << "/" << endFrame << " -> " << path << std::endl;
        }
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
