/*
** EPITECH PROJECT, 2024
** raytracer
** File description:
** CPURenderer
*/

#ifndef CPU_RENDERER_HPP_
#define CPU_RENDERER_HPP_

#include "plugins/renderers/IRenderer.hpp"
#include <string>

namespace RayTracer
{

class CPURenderer : public IRenderer
{
  public:
    CPURenderer(void) = default;
    ~CPURenderer(void) override = default;

    std::string getName(void) const override;
    std::string getDescription(void) const override;
    bool isAvailable(std::string &reason) const override;
    void run(const RenderContext &context) override;
};

} // namespace RayTracer

#endif // CPU_RENDERER_HPP_
