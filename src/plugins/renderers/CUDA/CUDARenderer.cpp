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
#include "scene/PrimitiveBounds.hpp"
#include <algorithm>
#include <chrono>
#include <cuda_runtime.h>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <limits>
#include <memory>
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
        converted.center = toDevice(primitive.center);
        converted.point = toDevice(primitive.point);
        converted.normal = toDevice(primitive.normal);
        converted.axis = toDevice(primitive.axis);
        converted.radius = primitive.radius;
        converted.height = primitive.height;
        converted.coneBaseAbove = primitive.coneBaseAbove ? 1 : 0;
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

HostBVH buildBVH(const SceneSnapshot &snapshot)
{
    struct Entry
    {
        int index;
        AABB bounds;
        Vec3D centroid;
    };

    HostBVH bvh;
    std::vector<Entry> entries;

    entries.reserve(snapshot.primitives.size());

    for (std::size_t i = 0; i < snapshot.primitives.size(); ++i)
    {
        AABB bounds;

        if (!computePrimitiveBounds(snapshot.primitives[i], bounds))
        {
            bvh.unbounded.push_back(static_cast<int>(i));
            continue;
        }

        entries.push_back(Entry{static_cast<int>(i), bounds, bounds.centroid()});
    }

    if (entries.size() < 8)
    {
        bvh.unbounded.clear();
        return bvh;
    }

    bvh.indices.reserve(entries.size());
    bvh.nodes.reserve(entries.size() * 2);

    auto buildNode = [&](auto &self, std::size_t start, std::size_t end) -> int {
        const int nodeIndex = static_cast<int>(bvh.nodes.size());

        bvh.nodes.emplace_back();

        AABB bounds;

        for (std::size_t i = start; i < end; ++i)
            bounds.expand(entries[i].bounds);

        Cuda::BVHNode node{};
        node.min = toDevice(bounds.min);
        node.max = toDevice(bounds.max);
        node.left = -1;
        node.right = -1;
        node.start = 0;
        node.count = 0;
        node.leaf = 0;

        if (end - start <= 4)
        {
            node.leaf = 1;
            node.start = static_cast<int>(bvh.indices.size());
            node.count = static_cast<int>(end - start);

            for (std::size_t i = start; i < end; ++i)
                bvh.indices.push_back(entries[i].index);
            bvh.nodes[static_cast<std::size_t>(nodeIndex)] = node;
            return nodeIndex;
        }

        const Vec3D extent = bounds.extent();
        int axis = 0;

        if (extent.getY() > extent.getX())
            axis = 1;
        if (extent.getZ() > (axis == 0 ? extent.getX() : extent.getY()))
            axis = 2;

        const std::size_t middle = (start + end) / 2;

        std::sort(entries.begin() + static_cast<std::ptrdiff_t>(start),
                  entries.begin() + static_cast<std::ptrdiff_t>(end),
                  [axis](const Entry &left, const Entry &right) {
                      return left.centroid[axis] < right.centroid[axis];
                  });

        node.left = self(self, start, middle);
        node.right = self(self, middle, end);
        bvh.nodes[static_cast<std::size_t>(nodeIndex)] = node;
        return nodeIndex;
    };

    buildNode(buildNode, 0, entries.size());
    return bvh;
}

int checkedCount(std::size_t count, const char *name)
{
    if (count > static_cast<std::size_t>(std::numeric_limits<int>::max()))
        throw std::runtime_error(std::string("Too many ") + name + " for the CUDA renderer");
    return static_cast<int>(count);
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
    explicit CudaRenderSession(const SceneSnapshot &snapshot, const PostProcessSettings &postProcess)
        : _width(snapshot.camera.imageWidth), _height(snapshot.camera.imageHeight),
          _primitives(convertPrimitives(snapshot)), _materials(convertMaterials(snapshot)),
          _lights(convertLights(snapshot)), _bvh(buildBVH(snapshot)), _devicePrimitives(_primitives.size()),
          _deviceMaterials(_materials.size()), _deviceLights(_lights.size()),
          _deviceNodes(_bvh.nodes.size()), _deviceIndices(_bvh.indices.size()),
          _deviceUnbounded(_bvh.unbounded.size()), _deviceAccumulation(pixelCount(_width, _height)),
          _deviceOutput(pixelCount(_width, _height) * 4u)
    {
        _devicePrimitives.upload(_primitives);
        _deviceMaterials.upload(_materials);
        _deviceLights.upload(_lights);
        _deviceNodes.upload(_bvh.nodes);
        _deviceIndices.upload(_bvh.indices);
        _deviceUnbounded.upload(_bvh.unbounded);

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
        _view.camera = toDevice(snapshot.camera);
        _view.background = toDeviceBackground(snapshot.background);
        _view.postProcess = toDevice(postProcess);

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
    DeviceBuffer<Cuda::Primitive> _devicePrimitives;
    DeviceBuffer<Cuda::Material> _deviceMaterials;
    DeviceBuffer<Cuda::Light> _deviceLights;
    DeviceBuffer<Cuda::BVHNode> _deviceNodes;
    DeviceBuffer<int> _deviceIndices;
    DeviceBuffer<int> _deviceUnbounded;
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

    cudaDeviceProp properties{};
    const cudaError_t propertiesStatus = cudaGetDeviceProperties(&properties, 0);

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

    std::string unavailableReason;

    if (!isAvailable(unavailableReason))
        throw std::runtime_error("CUDA renderer unavailable: " + unavailableReason);

    CameraController controller;
    controller.captureInitial(*context.camera);
    std::unique_ptr<CudaRenderSession> session;

    auto renderAll = [&](bool guiMode) {
        const SceneSnapshot snapshot = buildSceneSnapshot(*context.scene, *context.camera);

        if (!session || snapshot.camera.imageWidth != session->width() ||
            snapshot.camera.imageHeight != session->height())
            session = std::make_unique<CudaRenderSession>(snapshot, context.options.postProcess);
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

        while (start < totalSamples)
        {
            bool cameraChanged = false;
            char event;

            // Drain the event queue before every batch.
            while ((event = context.gui->getEvent()) != 0)
            {
                if (controller.handleEvent(event, *context.camera))
                    cameraChanged = true;
            }

            if (!context.gui->isOpen())
                return;

            if (cameraChanged)
            {
                const SceneSnapshot updated = buildSceneSnapshot(*context.scene, *context.camera);
                session->updateCamera(updated.camera);
                start = 0;
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
                    if (controller.handleEvent(pendingEvent, *context.camera))
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
                continue;
            }

            start += count;

            if (start == 1 || start % displayInterval == 0 || start >= totalSamples)
            {
                const std::vector<uint32_t> &pixels = session->copyImage();

                context.gui->setImage(session->width(), session->height(), pixels);
                context.gui->display();
            }
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
        char event;

        while ((event = context.gui->getEvent()) != 0)
        {
            if (controller.handleEvent(event, *context.camera))
                cameraChanged = true;
        }

        if (!context.gui->isOpen())
            break;

        if (cameraChanged)
        {
            std::clog << "Rendering image with CUDA." << std::endl;
            renderAll(true);
            if (context.gui->isOpen())
                std::clog << "Done.                                             " << std::endl;
        }
        else
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
