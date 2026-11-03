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
#include "render/PostProcess.hpp"
#include "scene/BVHBuilder.hpp"
#include "scene/Transform.hpp"
#include <algorithm>
#include <chrono>
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
    result.width = camera.imageWidth;
    result.height = camera.imageHeight;
    return result;
}

Cuda::PostProcess toDevice(const PostProcessSettings &settings)
{
    return Cuda::PostProcess{settings.exposure, static_cast<int>(settings.toneMapping), settings.srgb ? 1 : 0};
}

Cuda::Background toDeviceBackground(const Background &background)
{
    return Cuda::Background{toDevice(background.zenith), toDevice(background.horizon), toDevice(background.ground),
                            background.intensity, background.enabled ? 1 : 0};
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
        materials.push_back(converted);
    }
    return materials;
}

std::vector<Cuda::Light> convertLights(const SceneSnapshot &snapshot)
{
    std::vector<Cuda::Light> lights;

    lights.reserve(snapshot.lights.size());
    for (const LightData &light : snapshot.lights)
    {
        Cuda::Light converted;

        converted.type = static_cast<int>(light.type);
        converted.color = toDevice(light.color);
        converted.position = toDevice(light.position);
        converted.direction = toDevice(light.direction);
        converted.intensity = light.intensity;
        converted.attenuationConstant = light.attenuationConstant;
        converted.attenuationLinear = light.attenuationLinear;
        converted.attenuationQuadratic = light.attenuationQuadratic;
        lights.push_back(converted);
    }
    return lights;
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
                    Cuda::Triangle{toDevice(triangle.v0), toDevice(triangle.v1), toDevice(triangle.v2)});

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
          _devicePrimitives(_primitives.size()), _deviceMaterials(_materials.size()),
          _deviceLights(_lights.size()), _deviceNodes(_bvh.nodes.size()), _deviceIndices(_bvh.indices.size()),
          _deviceUnbounded(_bvh.unbounded.size()), _deviceMeshTriangles(_meshes.triangles.size()),
          _deviceMeshNodes(_meshes.nodes.size()), _deviceMeshIndices(_meshes.indices.size()),
          _deviceMeshRanges(_meshes.ranges.size()), _deviceAccumulation(pixelCount(_width, _height)),
          _deviceOutput(pixelCount(_width, _height) * 4u)
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
        _view.camera = toDevice(snapshot.camera);
        _view.background = toDeviceBackground(snapshot.background);
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

    void refreshOutput(int renderedSamples)
    {
        if (renderedSamples <= 0)
            return;

        finishBatch();
        Cuda::launchPostProcess(_view, _deviceAccumulation.get(), renderedSamples, _deviceOutput.get(), _stream);
        checkCuda(cudaGetLastError(), "post-process kernel launch");
        checkCuda(cudaStreamSynchronize(_stream), "post-process synchronize");
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
    int _width;
    int _height;
    std::vector<Cuda::Primitive> _primitives;
    std::vector<Cuda::Material> _materials;
    std::vector<Cuda::Light> _lights;
    HostBVH _bvh;
    HostMeshes _meshes;
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
    DeviceBuffer<float4> _deviceAccumulation;
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
        writePPMFile(context.options.outputPath, width, height, pixels);
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
