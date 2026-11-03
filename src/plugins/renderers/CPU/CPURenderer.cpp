/*
** EPITECH PROJECT, 2024
** raytracer
** File description:
** CPURenderer
*/

#include "plugins/renderers/CPU/CPURenderer.hpp"
#include "plugins/renderers/CPU/Renderer.hpp"

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
    if (context.camera->getImageWidth() != context.options.width ||
        context.camera->getImageHeight() != context.options.height)
    {
        context.camera->initialize(context.options.width, context.options.height, context.camera->getLookAt(),
                                   context.camera->getLookFrom(), context.camera->getVerticalFovDegree());
    }

    Renderer renderer(context.options.frames, context.scene, context.camera, context.gui, context.options.outputPath);
    renderer.run();
}

extern "C" std::shared_ptr<RayTracer::IRenderer> create(void)
{
    return std::make_shared<RayTracer::CPURenderer>();
}
