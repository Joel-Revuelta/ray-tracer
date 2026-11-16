/*
** EPITECH PROJECT, 2024
** raytracer
** File description:
** CUDARenderer
*/

#include "plugins/renderers/CUDA/CUDARenderer.hpp"
#include "plugins/renderers/CameraController.hpp"
#include "plugins/renderers/CUDA/CudaKernels.hpp"
#include "plugins/renderers/PPM.hpp"
#include "plugins/renderers/SceneSnapshot.hpp"
#include "render/ImageWriter.hpp"
#include "render/PostProcess.hpp"
#include "render/SequenceMetadata.hpp"
#include "render/Texture.hpp"
#include "render/VideoWriter.hpp"
#include "scene/BVHBuilder.hpp"
#include "scene/Transform.hpp"
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cuda_runtime.h>
#include <cstddef>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <limits>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

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

#ifndef RAYTRACER_CUDA_MIN_CC
#define RAYTRACER_CUDA_MIN_CC 89
#endif

static_assert(static_cast<int>(PrimitiveType::SPHERE) == Cuda::PRIMITIVE_SPHERE, "primitive enum mismatch");
static_assert(static_cast<int>(PrimitiveType::PLANE) == Cuda::PRIMITIVE_PLANE, "primitive enum mismatch");
static_assert(static_cast<int>(PrimitiveType::CYLINDER) == Cuda::PRIMITIVE_CYLINDER, "primitive enum mismatch");
static_assert(static_cast<int>(PrimitiveType::CONE) == Cuda::PRIMITIVE_CONE, "primitive enum mismatch");
static_assert(static_cast<int>(PrimitiveType::MESH) == Cuda::PRIMITIVE_MESH, "primitive enum mismatch");
static_assert(static_cast<int>(IMaterial::DIFFUSE) == Cuda::MATERIAL_DIFFUSE, "material enum mismatch");
static_assert(static_cast<int>(IMaterial::FLAT_COLOR) == Cuda::MATERIAL_FLAT_COLOR, "material enum mismatch");
static_assert(static_cast<int>(IMaterial::EMISSIVE) == Cuda::MATERIAL_EMISSIVE, "material enum mismatch");
static_assert(static_cast<int>(IMaterial::METALLIC) == Cuda::MATERIAL_METALLIC, "material enum mismatch");
static_assert(static_cast<int>(IMaterial::DIELECTRIC) == Cuda::MATERIAL_DIELECTRIC, "material enum mismatch");
static_assert(static_cast<int>(LightType::AMBIENT) == Cuda::LIGHT_AMBIENT, "light enum mismatch");
static_assert(static_cast<int>(LightType::DIRECTIONAL) == Cuda::LIGHT_DIRECTIONAL, "light enum mismatch");
static_assert(static_cast<int>(LightType::POINT) == Cuda::LIGHT_POINT, "light enum mismatch");
static_assert(static_cast<int>(LightType::AREA_SPHERE) == Cuda::LIGHT_AREA_SPHERE, "light enum mismatch");

namespace
{
void checkCuda(cudaError_t status, const char *operation)
{
    if (status == cudaSuccess)
        return;

    throw std::runtime_error(std::string("CUDA ") + operation + " failed: " + cudaGetErrorString(status));
}

template <typename T> class DeviceBuffer
{
  public:
    explicit DeviceBuffer(std::size_t count) : _count(count)
    {
        if (_count == 0)
            return;
        checkCuda(cudaMalloc(reinterpret_cast<void **>(&_data), _count * sizeof(T)), "cudaMalloc");
    }

    ~DeviceBuffer(void)
    {
        if (_data != nullptr)
            cudaFree(_data);
    }

    DeviceBuffer(const DeviceBuffer &) = delete;
    DeviceBuffer &operator=(const DeviceBuffer &) = delete;

    T *get(void) const
    {
        return _data;
    }

    void upload(const std::vector<T> &values)
    {
        if (values.size() != _count)
            throw std::runtime_error("CUDA buffer size mismatch");

        if (_count == 0)
            return;
        checkCuda(cudaMemcpy(_data, values.data(), _count * sizeof(T), cudaMemcpyHostToDevice), "cudaMemcpy");
    }

  private:
    T *_data = nullptr;
    std::size_t _count;
};

Cuda::Vec3 toDevice(const Vec3D &value)
{
    return Cuda::Vec3{value.getX(), value.getY(), value.getZ()};
}

Cuda::Camera toDevice(const CameraData &camera)
{
    Cuda::Camera result;

    result.lookFrom = toDevice(camera.lookFrom);
    result.initialPixelPos = toDevice(camera.initialPixelPos);
    result.pixelWidthDistance = toDevice(camera.pixelWidthDistance);
    result.pixelHeightDistance = toDevice(camera.pixelHeightDistance);
    result.viewRight = toDevice(camera.viewRight);
    result.viewUp = toDevice(camera.viewUp);
    result.width = camera.imageWidth;
    result.height = camera.imageHeight;
    result.lensRadius = camera.lensRadius;
    result.focusDistance = camera.focusDistance;
    result.focalDistance = camera.focalDistance;
    return result;
}

Cuda::PostProcess toDevice(const PostProcessSettings &settings)
{
    Cuda::PostProcess result{};

    result.exposure = settings.exposure;
    result.toneMapping = static_cast<int>(settings.toneMapping);
    result.srgb = settings.srgb ? 1 : 0;
    result.bloom = settings.bloom ? 1 : 0;
    result.bloomThreshold = settings.bloomThreshold;
    result.bloomIntensity = settings.bloomIntensity;
    result.bloomRadius = std::clamp(settings.bloomRadius, 1, BLOOM_MAX_RADIUS);

    const std::array<float, BLOOM_MAX_RADIUS * 2 + 1> weights = bloomKernel(result.bloomRadius);

    for (std::size_t i = 0; i < weights.size(); ++i)
        result.bloomWeights[i] = weights[i];
    return result;
}

Cuda::Background toDeviceBackground(const Background &background)
{
    return Cuda::Background{toDevice(background.zenith), toDevice(background.horizon), toDevice(background.ground),
                            background.intensity, background.enabled ? 1 : 0,
                            background.environmentTextureIndex};
}

std::vector<Cuda::Primitive> convertPrimitives(const SceneSnapshot &snapshot)
{
    std::vector<Cuda::Primitive> primitives;

    primitives.reserve(snapshot.primitives.size());
    for (const PrimitiveData &primitive : snapshot.primitives)
    {
        Cuda::Primitive converted;

        converted.type = static_cast<int>(primitive.type);
        converted.materialIndex = primitive.materialIndex;
        converted.meshIndex = static_cast<int>(primitive.meshIndex);
        converted.meshMaterialMapStart = primitive.meshMaterialMapStart;
        converted.meshMaterialMapCount = primitive.meshMaterialMapCount;
        converted.center = toDevice(primitive.center);
        converted.point = toDevice(primitive.point);
        converted.normal = toDevice(primitive.normal);
        converted.axis = toDevice(primitive.axis);
        converted.radius = primitive.radius;
        converted.height = primitive.height;
        converted.coneBaseAbove = primitive.coneBaseAbove ? 1 : 0;
        converted.transformed = isIdentityTransform(primitive.transform) ? 0 : 1;
        converted.transform = primitive.transform;
        primitives.push_back(converted);
    }
    return primitives;
}

std::vector<Cuda::Material> convertMaterials(const SceneSnapshot &snapshot)
{
    std::vector<Cuda::Material> materials;

    materials.reserve(snapshot.materials.size());
    for (const MaterialData &material : snapshot.materials)
    {
        Cuda::Material converted;

        converted.type = static_cast<int>(material.type);
        converted.diffuseColor = toDevice(material.diffuseColor);
        converted.emissionColor = toDevice(material.emissionColor);
        converted.ambientReflection = material.ambientReflection;
        converted.diffuseReflection = material.diffuseReflection;
        converted.specularReflection = material.specularReflection;
        converted.ior = material.ior;
        converted.roughness = material.roughness;
        converted.textureIndex = material.textureIndex;
        materials.push_back(converted);
    }
    return materials;
}

Cuda::Light toDevice(const LightData &light)
{
    Cuda::Light converted;

    converted.type = static_cast<int>(light.type);
    converted.color = toDevice(light.color);
    converted.position = toDevice(light.position);
    converted.direction = toDevice(light.direction);
    converted.radius = light.radius;
    converted.intensity = light.intensity;
    converted.attenuationConstant = light.attenuationConstant;
    converted.attenuationLinear = light.attenuationLinear;
    converted.attenuationQuadratic = light.attenuationQuadratic;
    return converted;
}

std::vector<Cuda::Light> convertLights(const std::vector<LightData> &source)
{
    std::vector<Cuda::Light> lights;

    lights.reserve(source.size());
    for (const LightData &light : source)
        lights.push_back(toDevice(light));
    return lights;
}

std::vector<Cuda::Light> convertLights(const SceneSnapshot &snapshot)
{
    return convertLights(snapshot.lights);
}

struct HostBVH
{
    std::vector<Cuda::BVHNode> nodes;
    std::vector<int> indices;
    std::vector<int> unbounded;
};

struct HostMeshes
{
    std::vector<Cuda::Triangle> triangles;
    std::vector<Cuda::BVHNode> nodes;
    std::vector<int> indices;
    std::vector<Cuda::MeshRange> ranges;
};

struct HostTextures
{
    std::vector<Cuda::Vec3> texels;
    std::vector<Cuda::TextureRange> ranges;
};

struct HostEnvironment
{
    std::vector<float> conditionalCdf;
    std::vector<float> marginalCdf;
    int width = 0;
    int height = 0;
    float total = 0.0f;
};

HostBVH buildBVH(const SceneSnapshot &snapshot)
{
    const BVHBuildResult result = RayTracer::buildBVH(snapshot.primitives);
    HostBVH bvh;

    bvh.nodes.reserve(result.nodes.size());

    for (const BVHNode &source : result.nodes)
    {
        Cuda::BVHNode node{};

        node.min = toDevice(source.bounds.min);
        node.max = toDevice(source.bounds.max);
        node.left = source.left;
        node.right = source.right;
        node.start = static_cast<int>(source.start);
        node.count = static_cast<int>(source.count);
        node.leaf = source.leaf ? 1 : 0;
        bvh.nodes.push_back(node);
    }

    bvh.indices.reserve(result.indices.size());

    for (const std::uint32_t index : result.indices)
        bvh.indices.push_back(static_cast<int>(index));

    bvh.unbounded.reserve(result.unbounded.size());

    for (const std::uint32_t index : result.unbounded)
        bvh.unbounded.push_back(static_cast<int>(index));

    return bvh;
}

int checkedCount(std::size_t count, const char *name)
{
    if (count > static_cast<std::size_t>(std::numeric_limits<int>::max()))
        throw std::runtime_error(std::string("Too many ") + name + " for the CUDA renderer");
    return static_cast<int>(count);
}

HostTextures convertTextures(const SceneSnapshot &snapshot)
{
    HostTextures textures;
    textures.ranges.reserve(snapshot.textures.size());

    for (const std::shared_ptr<const Texture> &texture : snapshot.textures)
    {
        Cuda::TextureRange range{};

        range.start = checkedCount(textures.texels.size(), "texture texels");
        range.width = texture ? texture->getWidth() : 0;
        range.height = texture ? texture->getHeight() : 0;

        if (texture)
        {
            for (const Color3D &color : texture->getPixels())
                textures.texels.push_back(toDevice(color));
        }

        textures.ranges.push_back(range);
    }

    return textures;
}

HostEnvironment convertEnvironment(const SceneSnapshot &snapshot)
{
    HostEnvironment environment;

    if (!snapshot.background.environmentDistribution)
        return environment;

    const EnvironmentDistribution &distribution = *snapshot.background.environmentDistribution;

    environment.conditionalCdf = distribution.conditionalCdf;
    environment.marginalCdf = distribution.marginalCdf;
    environment.width = distribution.width;
    environment.height = distribution.height;
    environment.total = distribution.total;
    return environment;
}

HostMeshes convertMeshes(const SceneSnapshot &snapshot)
{
    HostMeshes meshes;
    meshes.ranges.reserve(snapshot.meshes.size());

    for (const std::shared_ptr<const MeshAsset> &asset : snapshot.meshes)
    {
        Cuda::MeshRange range{};

        range.triangleStart = checkedCount(meshes.triangles.size(), "mesh triangles");
        range.nodeStart = checkedCount(meshes.nodes.size(), "mesh BVH nodes");
        range.indexStart = checkedCount(meshes.indices.size(), "mesh BVH indices");

        if (asset)
        {
            range.triangleCount = checkedCount(asset->triangles.size(), "mesh triangles");
            range.nodeCount = checkedCount(asset->bvh.nodes.size(), "mesh BVH nodes");
            range.indexCount = checkedCount(asset->bvh.indices.size(), "mesh BVH indices");

            for (const Triangle &triangle : asset->triangles)
                meshes.triangles.push_back(
                    Cuda::Triangle{toDevice(triangle.v0), toDevice(triangle.v1), toDevice(triangle.v2),
                                   toDevice(triangle.n0), toDevice(triangle.n1), toDevice(triangle.n2), triangle.tu0,
                                   triangle.tv0, triangle.tu1, triangle.tv1, triangle.tu2, triangle.tv2,
                                   triangle.materialGroup});

            for (const BVHNode &source : asset->bvh.nodes)
            {
                Cuda::BVHNode node{};

                node.min = toDevice(source.bounds.min);
                node.max = toDevice(source.bounds.max);
                node.left = source.left >= 0 ? source.left + range.nodeStart : -1;
                node.right = source.right >= 0 ? source.right + range.nodeStart : -1;
                node.start = static_cast<int>(source.start) + range.indexStart;
                node.count = static_cast<int>(source.count);
                node.leaf = source.leaf ? 1 : 0;
                meshes.nodes.push_back(node);
            }

            for (const std::uint32_t index : asset->bvh.indices)
                meshes.indices.push_back(range.triangleStart + static_cast<int>(index));
        }

        meshes.ranges.push_back(range);
    }

    return meshes;
}

std::size_t pixelCount(int width, int height)
{
    if (width <= 0 || height <= 0)
        throw std::invalid_argument("CUDA renderer received invalid image dimensions");
    return static_cast<std::size_t>(width) * static_cast<std::size_t>(height);
}

class CudaRenderSession
{
  public:
    explicit CudaRenderSession(const SceneSnapshot &snapshot, const PostProcessSettings &postProcess, uint32_t seed)
        : _width(snapshot.camera.imageWidth), _height(snapshot.camera.imageHeight),
          _primitives(convertPrimitives(snapshot)), _materials(convertMaterials(snapshot)),
          _lights(convertLights(snapshot)), _bvh(buildBVH(snapshot)), _meshes(convertMeshes(snapshot)),
          _textures(convertTextures(snapshot)), _environment(convertEnvironment(snapshot)),
          _meshMaterialMaps(snapshot.meshMaterialMaps),
          _devicePrimitives(_primitives.size()),
          _deviceMaterials(_materials.size()), _deviceLights(_lights.size()), _deviceNodes(_bvh.nodes.size()),
          _deviceIndices(_bvh.indices.size()), _deviceUnbounded(_bvh.unbounded.size()),
          _deviceMeshTriangles(_meshes.triangles.size()), _deviceMeshNodes(_meshes.nodes.size()),
          _deviceMeshIndices(_meshes.indices.size()), _deviceMeshRanges(_meshes.ranges.size()),
          _deviceTexels(_textures.texels.size()), _deviceTextureRanges(_textures.ranges.size()),
          _deviceMeshMaterialMaps(_meshMaterialMaps.size()),
          _deviceEnvironmentConditional(_environment.conditionalCdf.size()),
          _deviceEnvironmentMarginal(_environment.marginalCdf.size()),
          _deviceBloomA(postProcess.bloom ? pixelCount(_width, _height) : 0),
          _deviceBloomB(postProcess.bloom ? pixelCount(_width, _height) : 0),
          _deviceAccumulation(pixelCount(_width, _height)), _deviceOutput(pixelCount(_width, _height) * 4u)
    {
        _devicePrimitives.upload(_primitives);
        _deviceMaterials.upload(_materials);
        _deviceLights.upload(_lights);
        _deviceNodes.upload(_bvh.nodes);
        _deviceIndices.upload(_bvh.indices);
        _deviceUnbounded.upload(_bvh.unbounded);
        _deviceMeshTriangles.upload(_meshes.triangles);
        _deviceMeshNodes.upload(_meshes.nodes);
        _deviceMeshIndices.upload(_meshes.indices);
        _deviceMeshRanges.upload(_meshes.ranges);
        _deviceTexels.upload(_textures.texels);
        _deviceTextureRanges.upload(_textures.ranges);
        _deviceMeshMaterialMaps.upload(_meshMaterialMaps);
        _deviceEnvironmentConditional.upload(_environment.conditionalCdf);
        _deviceEnvironmentMarginal.upload(_environment.marginalCdf);

        _view.primitives = _devicePrimitives.get();
        _view.primitiveCount = checkedCount(_primitives.size(), "primitives");
        _view.materials = _deviceMaterials.get();
        _view.materialCount = checkedCount(_materials.size(), "materials");
        _view.lights = _deviceLights.get();
        _view.lightCount = checkedCount(_lights.size(), "lights");
        _view.nodes = _deviceNodes.get();
        _view.nodeCount = checkedCount(_bvh.nodes.size(), "BVH nodes");
        _view.primitiveIndices = _deviceIndices.get();
        _view.primitiveIndexCount = checkedCount(_bvh.indices.size(), "BVH primitive indices");
        _view.unboundedIndices = _deviceUnbounded.get();
        _view.unboundedCount = checkedCount(_bvh.unbounded.size(), "unbounded primitives");
        _view.meshTriangles = _deviceMeshTriangles.get();
        _view.meshTriangleCount = checkedCount(_meshes.triangles.size(), "mesh triangles");
        _view.meshNodes = _deviceMeshNodes.get();
        _view.meshNodeCount = checkedCount(_meshes.nodes.size(), "mesh BVH nodes");
        _view.meshIndices = _deviceMeshIndices.get();
        _view.meshIndexCount = checkedCount(_meshes.indices.size(), "mesh BVH indices");
        _view.meshRanges = _deviceMeshRanges.get();
        _view.meshCount = checkedCount(_meshes.ranges.size(), "meshes");
        _view.texels = _deviceTexels.get();
        _view.texelCount = checkedCount(_textures.texels.size(), "texture texels");
        _view.textures = _deviceTextureRanges.get();
        _view.textureCount = checkedCount(_textures.ranges.size(), "textures");
        _view.meshMaterialMaps = _deviceMeshMaterialMaps.get();
        _view.meshMaterialMapCount = checkedCount(_meshMaterialMaps.size(), "mesh material maps");
        _view.camera = toDevice(snapshot.camera);
        _view.background = toDeviceBackground(snapshot.background);
        _view.background.environmentConditionalCdf = _deviceEnvironmentConditional.get();
        _view.background.environmentMarginalCdf = _deviceEnvironmentMarginal.get();
        _view.background.environmentWidth = _environment.width;
        _view.background.environmentHeight = _environment.height;
        _view.background.environmentTotal = _environment.total;
        _view.postProcess = toDevice(postProcess);
        _view.seed = seed;

        checkCuda(cudaStreamCreateWithFlags(&_stream, cudaStreamNonBlocking), "cudaStreamCreate");
        try
        {
            checkCuda(cudaEventCreateWithFlags(&_event, cudaEventDisableTiming), "cudaEventCreate");
            checkCuda(cudaMemsetAsync(_deviceAccumulation.get(), 0,
                                      static_cast<std::size_t>(_width) * static_cast<std::size_t>(_height) *
                                          sizeof(float4),
                                      _stream),
                      "cudaMemsetAsync");
        }
        catch (...)
        {
            if (_event != nullptr)
                cudaEventDestroy(_event);
            cudaStreamDestroy(_stream);
            _event = nullptr;
            _stream = nullptr;
            throw;
        }
    }

    ~CudaRenderSession(void)
    {
        if (_batchInFlight && _event != nullptr)
            cudaEventSynchronize(_event);

        if (_hostPixelsPinned)
            cudaHostUnregister(_hostPixels.data());

        if (_event != nullptr)
            cudaEventDestroy(_event);
        if (_stream != nullptr)
            cudaStreamDestroy(_stream);
    }

    CudaRenderSession(const CudaRenderSession &) = delete;
    CudaRenderSession &operator=(const CudaRenderSession &) = delete;

    int width(void) const
    {
        return _width;
    }

    int height(void) const
    {
        return _height;
    }

    void updateCamera(const CameraData &camera)
    {
        finishBatch();
        _view.camera = toDevice(camera);
        checkCuda(cudaMemsetAsync(_deviceAccumulation.get(), 0,
                                  static_cast<std::size_t>(_width) * static_cast<std::size_t>(_height) * sizeof(float4),
                                  _stream),
                  "cudaMemsetAsync");
    }

    void setPostProcess(const PostProcessSettings &postProcess)
    {
        finishBatch();
        _view.postProcess = toDevice(postProcess);
    }

    void setSeed(uint32_t seed)
    {
        finishBatch();
        _view.seed = seed;
    }

    void updateLights(const std::vector<LightData> &lights)
    {
        finishBatch();
        if (lights.size() != _lights.size())
            throw std::runtime_error("CUDA light count cannot change during an animation");

        const std::vector<Cuda::Light> converted = convertLights(lights);

        _deviceLights.upload(converted);
        _lights = converted;
        _view.lights = _deviceLights.get();
        _view.lightCount = static_cast<int>(converted.size());
    }

    void refreshOutput(int renderedSamples)
    {
        if (renderedSamples <= 0)
            return;

        refreshOutputFrom(_deviceAccumulation.get(), renderedSamples);
    }

    void beginShutter(void)
    {
        finishBatch();
        if (!_deviceShutterAccumulation)
            _deviceShutterAccumulation =
                std::make_unique<DeviceBuffer<float4>>(pixelCount(_width, _height));

        checkCuda(cudaMemsetAsync(_deviceShutterAccumulation->get(), 0,
                                  pixelCount(_width, _height) * sizeof(float4), _stream),
                  "cudaMemsetAsync");
    }

    void accumulateShutter(void)
    {
        if (!_deviceShutterAccumulation)
            throw std::runtime_error("CUDA shutter accumulation was not initialized");

        finishBatch();
        Cuda::launchAccumulateShutter(_deviceAccumulation.get(), _deviceShutterAccumulation->get(),
                                      static_cast<int>(pixelCount(_width, _height)), _stream);
        checkCuda(cudaGetLastError(), "shutter accumulation kernel launch");
    }

    void refreshShutterOutput(int renderedSamples)
    {
        if (!_deviceShutterAccumulation)
            throw std::runtime_error("CUDA shutter output was not initialized");

        refreshOutputFrom(_deviceShutterAccumulation->get(), renderedSamples);
    }

    void launchBatch(int sampleStart, int sampleCount)
    {
        if (sampleCount <= 0)
            return;
        if (_batchInFlight)
            throw std::runtime_error("CUDA renderer attempted to launch a batch while another is running");

        Cuda::launchRender(_view, sampleStart, sampleCount, _deviceAccumulation.get(), _deviceOutput.get(), _stream);
        checkCuda(cudaGetLastError(), "kernel launch");

        const cudaError_t recordStatus = cudaEventRecord(_event, _stream);

        if (recordStatus != cudaSuccess)
        {
            cudaStreamSynchronize(_stream);
            checkCuda(recordStatus, "cudaEventRecord");
        }
        _batchInFlight = true;
    }

    bool isBatchComplete(void)
    {
        if (!_batchInFlight)
            return true;

        const cudaError_t status = cudaEventQuery(_event);

        if (status == cudaErrorNotReady)
            return false;
        checkCuda(status, "cudaEventQuery");
        return true;
    }

    void finishBatch(void)
    {
        if (!_batchInFlight)
            return;

        checkCuda(cudaEventSynchronize(_event), "cudaEventSynchronize");
        _batchInFlight = false;
        checkCuda(cudaGetLastError(), "kernel execution");
    }

    void renderBatch(int sampleStart, int sampleCount)
    {
        launchBatch(sampleStart, sampleCount);
        finishBatch();
    }

    const std::vector<uint32_t> &copyImage(void)
    {
        finishBatch();

        const std::size_t count = pixelCount(_width, _height);

        if (_hostPixels.size() != count)
        {
            if (_hostPixelsPinned)
            {
                cudaHostUnregister(_hostPixels.data());
                _hostPixelsPinned = false;
            }

            _hostPixels.resize(count);

            // Pinned host memory makes the device-to-host readback faster. If
            // registration is unavailable, the pageable vector still works.
            if (cudaHostRegister(_hostPixels.data(), _hostPixels.size() * sizeof(uint32_t),
                                 cudaHostRegisterDefault) == cudaSuccess)
                _hostPixelsPinned = true;
            else
                cudaGetLastError();
        }

        checkCuda(cudaMemcpyAsync(_hostPixels.data(), _deviceOutput.get(), _hostPixels.size() * sizeof(uint32_t),
                                  cudaMemcpyDeviceToHost, _stream),
                  "cudaMemcpyAsync");
        checkCuda(cudaStreamSynchronize(_stream), "cudaStreamSynchronize");
        return _hostPixels;
    }

  private:
    void refreshOutputFrom(const float4 *accumulation, int renderedSamples)
    {
        if (renderedSamples <= 0)
            return;

        finishBatch();
        Cuda::launchPostProcess(_view, accumulation, renderedSamples, _deviceBloomA.get(),
                                _deviceBloomB.get(), _deviceOutput.get(), _stream);
        checkCuda(cudaGetLastError(), "post-process kernel launch");
        checkCuda(cudaStreamSynchronize(_stream), "post-process synchronize");
    }

    int _width;
    int _height;
    std::vector<Cuda::Primitive> _primitives;
    std::vector<Cuda::Material> _materials;
    std::vector<Cuda::Light> _lights;
    HostBVH _bvh;
    HostMeshes _meshes;
    HostTextures _textures;
    HostEnvironment _environment;
    std::vector<int> _meshMaterialMaps;
    DeviceBuffer<Cuda::Primitive> _devicePrimitives;
    DeviceBuffer<Cuda::Material> _deviceMaterials;
    DeviceBuffer<Cuda::Light> _deviceLights;
    DeviceBuffer<Cuda::BVHNode> _deviceNodes;
    DeviceBuffer<int> _deviceIndices;
    DeviceBuffer<int> _deviceUnbounded;
    DeviceBuffer<Cuda::Triangle> _deviceMeshTriangles;
    DeviceBuffer<Cuda::BVHNode> _deviceMeshNodes;
    DeviceBuffer<int> _deviceMeshIndices;
    DeviceBuffer<Cuda::MeshRange> _deviceMeshRanges;
    DeviceBuffer<Cuda::Vec3> _deviceTexels;
    DeviceBuffer<Cuda::TextureRange> _deviceTextureRanges;
    DeviceBuffer<int> _deviceMeshMaterialMaps;
    DeviceBuffer<float> _deviceEnvironmentConditional;
    DeviceBuffer<float> _deviceEnvironmentMarginal;
    DeviceBuffer<float4> _deviceBloomA;
    DeviceBuffer<float4> _deviceBloomB;
    DeviceBuffer<float4> _deviceAccumulation;
    std::unique_ptr<DeviceBuffer<float4>> _deviceShutterAccumulation;
    DeviceBuffer<unsigned char> _deviceOutput;
    std::vector<uint32_t> _hostPixels;
    bool _hostPixelsPinned = false;
    Cuda::SceneView _view;
    cudaStream_t _stream = nullptr;
    cudaEvent_t _event = nullptr;
    bool _batchInFlight = false;
};

void writeOutput(const RenderContext &context, int width, int height, const std::vector<uint32_t> &pixels)
{
    if (!context.options.outputPath.empty())
        writeImageFileAtomic(context.options.outputPath, width, height, pixels);
    else if (!context.gui)
        writePPM(std::cout, width, height, pixels);
}

void saveScreenshot(const std::vector<uint32_t> &pixels, int width, int height)
{
    const auto stamp = std::chrono::duration_cast<std::chrono::milliseconds>(
                           std::chrono::system_clock::now().time_since_epoch())
                           .count();
    const std::string path = "raytracer-" + std::to_string(stamp) + ".ppm";

    writePPMFile(path, width, height, pixels);
    std::clog << "\rSaved screenshot: " << path << std::endl;
}
} // namespace

std::string CUDARenderer::getName(void) const
{
    return "cuda";
}

std::string CUDARenderer::getDescription(void) const
{
    return "CUDA GPU renderer";
}

bool CUDARenderer::isAvailable(std::string &reason) const
{
    int deviceCount = 0;
    const cudaError_t status = cudaGetDeviceCount(&deviceCount);

    if (status != cudaSuccess)
    {
        reason = cudaGetErrorString(status);
        return false;
    }
    if (deviceCount <= 0)
    {
        reason = "no CUDA device found";
        return false;
    }

    int currentDevice = 0;
    const cudaError_t currentDeviceStatus = cudaGetDevice(&currentDevice);

    if (currentDeviceStatus != cudaSuccess)
    {
        reason = cudaGetErrorString(currentDeviceStatus);
        return false;
    }

    cudaDeviceProp properties{};
    const cudaError_t propertiesStatus = cudaGetDeviceProperties(&properties, currentDevice);

    if (propertiesStatus != cudaSuccess)
    {
        reason = cudaGetErrorString(propertiesStatus);
        return false;
    }

    const int capability = properties.major * 10 + properties.minor;

    if (capability < RAYTRACER_CUDA_MIN_CC)
    {
        reason = "GPU compute capability " + std::to_string(properties.major) + "." +
                 std::to_string(properties.minor) + " is below the renderer minimum " +
                 std::to_string(RAYTRACER_CUDA_MIN_CC / 10) + "." +
                 std::to_string(RAYTRACER_CUDA_MIN_CC % 10);
        return false;
    }

    return true;
}

void CUDARenderer::run(const RenderContext &context)
{
    if (!context.scene || !context.camera)
        throw std::invalid_argument("CUDA renderer requires a scene and camera");
    if (context.options.frames <= 0)
        throw std::invalid_argument("CUDA renderer requires at least one sample");

    if (context.camera->getImageWidth() != context.options.width ||
        context.camera->getImageHeight() != context.options.height)
    {
        context.camera->initialize(context.options.width, context.options.height, context.camera->getLookAt(),
                                   context.camera->getLookFrom(), context.camera->getVerticalFovDegree());
    }

    int deviceCount = 0;

    checkCuda(cudaGetDeviceCount(&deviceCount), "cudaGetDeviceCount");
    if (context.options.device < 0 || context.options.device >= deviceCount)
        throw std::invalid_argument("CUDA device index is out of range");
    checkCuda(cudaSetDevice(context.options.device), "cudaSetDevice");

    cudaDeviceProp selectedDevice{};

    checkCuda(cudaGetDeviceProperties(&selectedDevice, context.options.device), "cudaGetDeviceProperties");
    if (selectedDevice.major * 10 + selectedDevice.minor < RAYTRACER_CUDA_MIN_CC)
        throw std::runtime_error("Selected CUDA device is below the renderer compute capability minimum");

    std::string unavailableReason;

    if (!isAvailable(unavailableReason))
        throw std::runtime_error("CUDA renderer unavailable: " + unavailableReason);

    if (context.options.animation)
    {
        if (!context.animation)
            throw std::invalid_argument("CUDA animation requires a camera animation");
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
        const bool writeFrames = !context.options.outputPath.empty();
        const std::string pattern = writeFrames ? context.options.outputPath : std::string();
        const bool writeVideo = !context.options.videoPath.empty();
        const bool sceneInstances = context.sceneAnimation && context.sceneAnimation->animatesInstances();
        const bool sceneLights = context.sceneAnimation && context.sceneAnimation->animatesLights();

        if (!writeFrames && !writeVideo)
            throw std::invalid_argument("CUDA animation requires a frame pattern or a video path");
        if (writeVideo && context.options.resume)
            throw std::invalid_argument("CUDA video output cannot be resumed");

        std::unique_ptr<VideoWriter> video;

        if (writeVideo)
        {
            const VideoSettings videoSettings{context.options.videoPath, context.options.videoCodec,
                                              context.options.videoCrf,   context.options.videoPreset,
                                              context.options.videoBitrate, fps, context.options.width,
                                              context.options.height};

            video = std::make_unique<VideoWriter>(videoSettings);
        }

        auto frameSeed = [&](int frame) {
            return context.options.seed + static_cast<uint32_t>(frame) * 0x9e3779b9u;
        };
        auto subframeSeed = [&](int frame, int subframe) {
            uint32_t seed = frameSeed(frame);

            if (subframes > 1)
                seed += static_cast<uint32_t>(subframe) * 0x85ebca6bu;
            return seed;
        };

        auto writeMetadata = [&]() {
            SequenceMetadata metadata;

            metadata.scenePath = context.options.scenePath;
            metadata.renderer = context.options.rendererName;
            metadata.width = context.options.width;
            metadata.height = context.options.height;
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
                const bool rendered = writeFrames ? frameFileExists(path) : true;

                metadata.frames.push_back(
                    makeFrameMetadata(frame, frameTime, path, frameSeed(frame), *camera, rendered));
            }

            const std::string metadataPath =
                writeFrames ? sequenceMetadataPath(pattern) : sequenceMetadataPath(context.options.videoPath);

            writeSequenceMetadata(metadataPath, metadata);
            std::clog << "Wrote sequence metadata -> " << metadataPath << std::endl;
        };

        int firstRendered = startFrame;

        if (writeFrames && context.options.resume)
        {
            while (firstRendered <= endFrame && frameFileExists(formatFramePath(pattern, firstRendered)))
                ++firstRendered;

            if (firstRendered > endFrame)
            {
                std::clog << "All animation frames already exist; leaving metadata unchanged." << std::endl;
                return;
            }
        }

        std::shared_ptr<Camera> firstCamera =
            context.animation->cameraAt(context.animation->timeAtFrame(firstRendered, fps));
        firstCamera->initialize(context.options.width, context.options.height, firstCamera->getLookAt(),
                                firstCamera->getLookFrom(), firstCamera->getVerticalFovDegree());

        if (context.options.lensRadiusOverride >= 0.0f)
            firstCamera->setLensRadius(context.options.lensRadiusOverride);
        if (context.options.focusDistanceOverride > 0.0f)
            firstCamera->setFocusDistance(context.options.focusDistanceOverride);

        std::unique_ptr<CudaRenderSession> session;

        if (!sceneInstances)
        {
            const SceneSnapshot snapshot = buildSceneSnapshot(*context.scene, *firstCamera);

            session = std::make_unique<CudaRenderSession>(snapshot, context.options.postProcess,
                                                          subframeSeed(firstRendered, 0));
        }

        const auto sequenceStart = std::chrono::steady_clock::now();
        int completedFrames = 0;

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
                context.sceneAnimation->apply(*context.scene, frameTime);

            std::unique_ptr<CudaRenderSession> frameSession;
            CudaRenderSession *active = session.get();

            if (sceneInstances)
            {
                std::shared_ptr<Camera> nominalCamera = context.animation->cameraAt(frameTime);

                nominalCamera->initialize(context.options.width, context.options.height, nominalCamera->getLookAt(),
                                          nominalCamera->getLookFrom(), nominalCamera->getVerticalFovDegree());

                if (context.options.lensRadiusOverride >= 0.0f)
                    nominalCamera->setLensRadius(context.options.lensRadiusOverride);
                if (context.options.focusDistanceOverride > 0.0f)
                    nominalCamera->setFocusDistance(context.options.focusDistanceOverride);

                const SceneSnapshot frameSnapshot = buildSceneSnapshot(*context.scene, *nominalCamera);

                frameSession = std::make_unique<CudaRenderSession>(frameSnapshot, context.options.postProcess,
                                                                  subframeSeed(frame, 0));
                active = frameSession.get();
            }
            else if (sceneLights)
            {
                active->updateLights(collectLights(*context.scene));
            }

            if (!active)
                throw std::runtime_error("CUDA animation session was not initialized");

            if (motionBlur)
                active->beginShutter();

            for (int subframe = 0; subframe < subframes; ++subframe)
            {
                std::shared_ptr<Camera> camera = context.animation->cameraAt(
                    shutterSampleTime(frameTime, fps, shutter, subframe));

                camera->initialize(context.options.width, context.options.height, camera->getLookAt(),
                                   camera->getLookFrom(), camera->getVerticalFovDegree());

                if (context.options.lensRadiusOverride >= 0.0f)
                    camera->setLensRadius(context.options.lensRadiusOverride);
                if (context.options.focusDistanceOverride > 0.0f)
                    camera->setFocusDistance(context.options.focusDistanceOverride);

                active->updateCamera(makeCameraData(*camera));
                active->setSeed(subframeSeed(frame, subframe));

                constexpr int batchSize = 16;

                for (int start = 0; start < context.options.frames; start += batchSize)
                {
                    const int count = std::min(batchSize, context.options.frames - start);

                    active->renderBatch(start, count);
                }

                if (motionBlur)
                    active->accumulateShutter();
            }

            if (motionBlur)
                active->refreshShutterOutput(context.options.frames * subframes);
            else if (context.options.postProcess.bloom)
                active->refreshOutput(context.options.frames);

            const std::vector<uint32_t> &pixels = active->copyImage();

            if (writeFrames)
                writeImageFileAtomic(path, active->width(), active->height(), pixels);
            if (video)
                video->writeFrame(pixels);
            ++completedFrames;

            const double elapsed =
                std::chrono::duration<double>(std::chrono::steady_clock::now() - sequenceStart).count();
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
                      << ", " << context.options.width << "x" << context.options.height << ", " << fps << " fps)"
                      << std::endl;
        }

        writeMetadata();
        return;
    }

    CameraController controller;
    controller.captureInitial(*context.camera);
    PostProcessSettings postProcess = context.options.postProcess;
    std::unique_ptr<CudaRenderSession> session;
    const auto renderStart = std::chrono::steady_clock::now();

    auto updateTitle = [&](int completedSamples, int totalSamples, bool paused) {
        if (!context.gui)
            return;

        const double elapsed =
            std::chrono::duration<double>(std::chrono::steady_clock::now() - renderStart).count();
        std::ostringstream title;

        title << "RayTracer CUDA | " << context.options.width << "x" << context.options.height << " | "
              << completedSamples << "/" << totalSamples << " spp | " << std::fixed << std::setprecision(1) << elapsed
              << "s | exposure " << std::setprecision(2) << postProcess.exposure;

        if (context.camera->getLensRadius() > 0.0f)
            title << " | lens " << context.camera->getLensRadius() << " | focus "
                  << context.camera->getFocusDistance();

        if (paused)
            title << " | paused";

        context.gui->setTitle(title.str());
    };

    auto renderAll = [&](bool guiMode) {
        const SceneSnapshot snapshot = buildSceneSnapshot(*context.scene, *context.camera);

        if (!session || snapshot.camera.imageWidth != session->width() ||
            snapshot.camera.imageHeight != session->height())
            session = std::make_unique<CudaRenderSession>(snapshot, postProcess, context.options.seed);
        else
            session->updateCamera(snapshot.camera);

        const int totalSamples = context.options.frames;

        if (!guiMode)
        {
            constexpr int batchSize = 16;

            for (int start = 0; start < totalSamples; start += batchSize)
            {
                const int count = std::min(batchSize, totalSamples - start);

                session->renderBatch(start, count);
                std::clog << "\rRendering image with CUDA: " << (100 * (start + count) / totalSamples) << "% "
                          << std::flush;
            }
            std::clog << std::endl;

            if (postProcess.bloom)
                session->refreshOutput(totalSamples);

            const std::vector<uint32_t> &pixels = session->copyImage();

            writeOutput(context, session->width(), session->height(), pixels);
            return;
        }

        constexpr int batchSize = 1;
        // Cap progressive uploads at roughly 100 frames. Copying a full 8K
        // image after every sample would dominate the render time.
        const int displayInterval = std::max(1, totalSamples / 100);
        int start = 0;
        bool paused = false;
        bool postProcessDirty = false;
        bool screenshotRequested = false;

        auto handleEvent = [&](char event, bool &cameraChanged) {
            if (!event)
                return;

            if (event == ' ')
            {
                paused = !paused;
                updateTitle(start, totalSamples, paused);
                return;
            }
            if (event == 'p' || event == 'P')
            {
                screenshotRequested = true;
                return;
            }
            if (event == '[' || event == ']')
            {
                const float factor = event == ']' ? 1.1f : 1.0f / 1.1f;

                postProcess.exposure =
                    std::clamp(postProcess.exposure * factor, 0.05f, 20.0f);
                postProcessDirty = true;
                updateTitle(start, totalSamples, paused);
                return;
            }
            if (controller.handleEvent(event, *context.camera))
                cameraChanged = true;
        };

        updateTitle(start, totalSamples, paused);

        while (start < totalSamples)
        {
            bool cameraChanged = false;
            char event;

            // Drain the event queue before every batch.
            while ((event = context.gui->getEvent()) != 0)
                handleEvent(event, cameraChanged);

            if (!context.gui->isOpen())
                return;

            if (cameraChanged)
            {
                const SceneSnapshot updated = buildSceneSnapshot(*context.scene, *context.camera);
                session->updateCamera(updated.camera);
                start = 0;
                updateTitle(start, totalSamples, paused);
                continue;
            }

            if (postProcessDirty)
            {
                session->setPostProcess(postProcess);

                if (start > 0)
                {
                    session->refreshOutput(start);
                    const std::vector<uint32_t> &pixels = session->copyImage();

                    context.gui->setImage(session->width(), session->height(), pixels);
                    context.gui->display();
                }
                postProcessDirty = false;
            }

            if (screenshotRequested)
            {
                if (start > 0)
                {
                    const std::vector<uint32_t> &pixels = session->copyImage();

                    saveScreenshot(pixels, session->width(), session->height());
                }
                screenshotRequested = false;
            }

            if (paused)
            {
                context.gui->display();
                std::this_thread::sleep_for(std::chrono::milliseconds(8));
                continue;
            }

            const int count = std::min(batchSize, totalSamples - start);
            session->launchBatch(start, count);

            bool restart = false;

            // Keep pumping events while the GPU is working. Heavy scenes can
            // make a sample take a long time; polling here prevents the window
            // manager from marking the app as unresponsive.
            while (!session->isBatchComplete())
            {
                char pendingEvent;

                while ((pendingEvent = context.gui->getEvent()) != 0)
                {
                    bool pendingCameraChange = false;

                    handleEvent(pendingEvent, pendingCameraChange);
                    if (pendingCameraChange)
                        restart = true;
                }

                if (!context.gui->isOpen())
                {
                    session->finishBatch();
                    return;
                }
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            }

            session->finishBatch();

            if (restart)
            {
                std::clog << "\rCamera moved; restarting CUDA render." << std::endl;
                const SceneSnapshot updated = buildSceneSnapshot(*context.scene, *context.camera);
                session->updateCamera(updated.camera);
                start = 0;
                updateTitle(start, totalSamples, paused);
                continue;
            }

            start += count;

            bool refreshed = false;

            if (postProcessDirty)
            {
                session->setPostProcess(postProcess);
                session->refreshOutput(start);
                postProcessDirty = false;
                refreshed = true;
            }

            if (postProcess.bloom && !refreshed)
            {
                session->refreshOutput(start);
                refreshed = true;
            }

            const bool displayFrame = refreshed || start == 1 || start % displayInterval == 0 || start >= totalSamples;

            if (displayFrame || screenshotRequested)
            {
                const std::vector<uint32_t> &pixels = session->copyImage();

                if (displayFrame)
                {
                    context.gui->setImage(session->width(), session->height(), pixels);
                    context.gui->display();
                }
                if (screenshotRequested)
                {
                    saveScreenshot(pixels, session->width(), session->height());
                    screenshotRequested = false;
                }
            }

            updateTitle(start, totalSamples, paused);
        }

        if (!context.options.outputPath.empty())
        {
            const std::vector<uint32_t> &pixels = session->copyImage();

            writePPMFile(context.options.outputPath, session->width(), session->height(), pixels);
        }
    };

    std::clog << "Rendering image with CUDA." << std::endl;

    if (!context.gui)
    {
        renderAll(false);
        std::clog << "Done.                                             " << std::endl;
        return;
    }

    renderAll(true);

    if (!context.gui->isOpen())
        return;

    std::clog << "Done.                                             " << std::endl;

    while (context.gui->isOpen())
    {
        bool cameraChanged = false;
        bool exposureChanged = false;
        bool screenshotRequested = false;
        char event;

        while ((event = context.gui->getEvent()) != 0)
        {
            if (event == 'p' || event == 'P')
                screenshotRequested = true;
            else if (event == '[' || event == ']')
            {
                const float factor = event == ']' ? 1.1f : 1.0f / 1.1f;

                postProcess.exposure =
                    std::clamp(postProcess.exposure * factor, 0.05f, 20.0f);
                exposureChanged = true;
            }
            else if (controller.handleEvent(event, *context.camera))
                cameraChanged = true;
        }

        if (!context.gui->isOpen())
            break;

        if (exposureChanged && session)
        {
            session->setPostProcess(postProcess);
            session->refreshOutput(context.options.frames);
            const std::vector<uint32_t> &pixels = session->copyImage();

            context.gui->setImage(session->width(), session->height(), pixels);
            context.gui->display();
            updateTitle(context.options.frames, context.options.frames, false);
        }

        if (screenshotRequested && session)
        {
            const std::vector<uint32_t> &pixels = session->copyImage();

            saveScreenshot(pixels, session->width(), session->height());
        }

        if (cameraChanged)
        {
            std::clog << "Rendering image with CUDA." << std::endl;
            renderAll(true);
            if (context.gui->isOpen())
                std::clog << "Done.                                             " << std::endl;
        }
        else if (!exposureChanged)
        {
            context.gui->display();
            std::this_thread::sleep_for(std::chrono::milliseconds(8));
        }
    }
}

extern "C" std::shared_ptr<RayTracer::IRenderer> create(void)
{
    return std::make_shared<RayTracer::CUDARenderer>();
}
