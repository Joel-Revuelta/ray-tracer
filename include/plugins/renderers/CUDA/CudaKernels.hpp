/*
** EPITECH PROJECT, 2024
** raytracer
** File description:
** CudaKernels
*/

#ifndef CUDA_KERNELS_HPP_
#define CUDA_KERNELS_HPP_

#include "plugins/renderers/CUDA/CudaScene.hpp"
#include <cuda_runtime.h>

namespace RayTracer
{
namespace Cuda
{

void launchRender(const SceneView &scene, int sampleStart, int sampleCount, float4 *accumulation, unsigned char *output,
                  cudaStream_t stream);

void launchPostProcess(const SceneView &scene, const float4 *accumulation, int renderedSamples, unsigned char *output,
                       cudaStream_t stream);

} // namespace Cuda
} // namespace RayTracer

#endif // CUDA_KERNELS_HPP_
