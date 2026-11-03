/*
** EPITECH PROJECT, 2024
** raytracer
** File description:
** IRenderer
*/

#ifndef I_RENDERER_HPP_
#define I_RENDERER_HPP_

#include "Camera.hpp"
#include "GUI.hpp"
#include "Scene.hpp"
#include <memory>
#include <string>

namespace RayTracer
{

struct RenderOptions
{
    int width = 0;
    int height = 0;
    int frames = 1;
    std::string outputPath;
};

struct RenderContext
{
    std::shared_ptr<Scene> scene;
    std::shared_ptr<Camera> camera;
    std::shared_ptr<IGUI> gui;
    RenderOptions options;
};

class IRenderer
{
  public:
    virtual ~IRenderer(void) = default;

    virtual std::string getName(void) const = 0;
    virtual std::string getDescription(void) const = 0;
    virtual bool isAvailable(std::string &reason) const = 0;
    virtual void run(const RenderContext &context) = 0;
};

} // namespace RayTracer

#endif // I_RENDERER_HPP_
