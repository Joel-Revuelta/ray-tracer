/*
** EPITECH PROJECT, 2024
** raytracer
** File description:
** CUDARenderer
*/

#ifndef CUDA_RENDERER_HPP_
#define CUDA_RENDERER_HPP_

#include "plugins/renderers/IRenderer.hpp"
#include <string>

namespace RayTracer
{

class CUDARenderer : public IRenderer
{
  public:
    CUDARenderer(void) = default;
    ~CUDARenderer(void) override = default;

    std::string getName(void) const override;
    std::string getDescription(void) const override;
    bool isAvailable(std::string &reason) const override;
    void run(const RenderContext &context) override;
};

} // namespace RayTracer

#endif // CUDA_RENDERER_HPP_
