/*
** EPITECH PROJECT, 2023
** raytracer
** File description:
** Renderer.cpp
*/

#include "plugins/renderers/CPU/Renderer.hpp"
#include "render/Texture.hpp"
#include "Utils.hpp"
#include "render/Brdf.hpp"
#include "plugins/renderers/PPM.hpp"
#include <algorithm>
#include <chrono>
#include <cmath>
#include <condition_variable>
#include <cstdint>
#include <exception>
#include <functional>
#include <iomanip>
#include <mutex>
#include <sstream>
#include <stdexcept>
#include <thread>
#include <utility>

using namespace RayTracer;

namespace RayTracer
{
class RenderPool
{
  public:
    explicit RenderPool(std::size_t count)
    {
        resize(count);
    }

    ~RenderPool(void)
    {
        stop();
    }

    RenderPool(const RenderPool &) = delete;
    RenderPool &operator=(const RenderPool &) = delete;

    void resize(std::size_t count)
    {
        if (_workers.size() == count)
            return;

        stop();
        _stop = false;
        _workers.reserve(count);

        try
        {
            for (std::size_t i = 0; i < count; ++i)
                _workers.emplace_back(&RenderPool::worker, this, i);
        }
        catch (...)
        {
            {
                std::lock_guard<std::mutex> lock(_mutex);
                _stop = true;
            }
            _condition.notify_all();

            for (std::thread &workerThread : _workers)
            {
                if (workerThread.joinable())
                    workerThread.join();
            }
            _workers.clear();
            throw;
        }
    }

    void run(const std::function<void(int, int)> &task, const std::vector<std::pair<int, int>> &ranges)
    {
        if (_workers.empty())
            return;

        {
            std::lock_guard<std::mutex> lock(_mutex);

            _task = task;
            _ranges = ranges;
            _remaining.store(static_cast<int>(_workers.size()), std::memory_order_release);
            ++_generation;
        }
        _condition.notify_all();
    }

    bool running(void) const
    {
        return _remaining.load(std::memory_order_acquire) > 0;
    }

    void wait(void)
    {
        while (running())
            std::this_thread::sleep_for(std::chrono::milliseconds(1));

        {
            std::lock_guard<std::mutex> lock(_mutex);
            _task = nullptr;
            _ranges.clear();
        }
    }

  private:
    void stop(void)
    {
        {
            std::lock_guard<std::mutex> lock(_mutex);

            _stop = true;
            _task = nullptr;
            _ranges.clear();
            _generation = 0;
        }
        _condition.notify_all();

        for (std::thread &workerThread : _workers)
        {
            if (workerThread.joinable())
                workerThread.join();
        }
        _workers.clear();
        _remaining.store(0, std::memory_order_release);
        _stop = false;
    }

    void worker(std::size_t index)
    {
        std::uint64_t seenGeneration = 0;

        while (true)
        {
            std::function<void(int, int)> task;
            int startY = 0;
            int endY = 0;

            {
                std::unique_lock<std::mutex> lock(_mutex);

                _condition.wait(lock, [&] { return _stop || _generation != seenGeneration; });

                if (_stop)
                    return;

                seenGeneration = _generation;
                task = _task;

                if (index < _ranges.size())
                {
                    startY = _ranges[index].first;
                    endY = _ranges[index].second;
                }
            }

            if (task)
                task(startY, endY);

            _remaining.fetch_sub(1, std::memory_order_release);
        }
    }

    std::vector<std::thread> _workers;
    std::vector<std::pair<int, int>> _ranges;
    std::function<void(int, int)> _task;
    std::mutex _mutex;
    std::condition_variable _condition;
    std::atomic<int> _remaining{0};
    std::uint64_t _generation = 0;
    bool _stop = false;
};
} // namespace RayTracer

Renderer::Renderer(void)
    : _frameIterations(100), _frameIterationsScale(1.0 / _frameIterations), _scene(nullptr), _camera(nullptr),
      _gui(nullptr), _postProcess()
{
}

Renderer::Renderer(int raysPerPixel, std::shared_ptr<Scene> scene, std::shared_ptr<Camera> camera,
                   std::shared_ptr<IGUI> gui, const std::string &outFile, const PostProcessSettings &postProcess,
                   std::size_t threads, uint32_t seed)
    : _frameIterations(raysPerPixel), _frameIterationsScale(1.0 / _frameIterations), _scene(scene), _camera(camera),
      _gui(gui), _postProcess(postProcess), _threadCount(threads), _seed(seed)
{
    if (raysPerPixel <= 0)
        throw std::invalid_argument("Frame iterations must be positive");

    if (!outFile.empty())
    {
        _fileStream.open(outFile, std::ios::out | std::ios::binary);
        if (!_fileStream.is_open())
            throw std::runtime_error("Cannot open output file: " + outFile);
        _outStream = &_fileStream;
    }
    else if (!gui)
        _outStream = &std::cout;

    initialize();
}

Renderer::~Renderer(void)
{
}

/**
 * Initialized the _imageData and _imageAccumulationData vectors with the correct size and sets the base
 * ray directions for each pixel in the image
 */
void Renderer::initialize(void)
{
    const int imageWidth = getCamera()->getImageWidth();
    const int imageHeight = getCamera()->getImageHeight();
    const std::size_t pixelCount =
        static_cast<std::size_t>(imageWidth) * static_cast<std::size_t>(imageHeight);

    // Initialize the imageData to 0.
    _imageData.clear();
    _imageData.resize(pixelCount);

    // Initialize the _imageAccumulationData to basic Color4D.
    _imageAccumulationData.clear();
    _imageAccumulationData.resize(pixelCount);

    // Generate the basic ray directions for each pixel in the image.
    _rayDirections.clear();
    for (int y = 0; y < imageHeight; y++)
    {
        for (int x = 0; x < imageWidth; x++)
        {
            Vec3D positionToHit = getCamera()->getInitialPixelPos() + (x * getCamera()->getPixelWidthDistance()) +
                                  (y * getCamera()->getPixelHeightDistance());
            Vec3D rayDirection = positionToHit - getCamera()->getLookFrom();
            _rayDirections.push_back(rayDirection);
        }
    }
}

void Renderer::run(void)
{
    if (getCamera())
        _cameraController.captureInitial(*getCamera());

    render();
    std::clog << std::endl;

    if (!_gui)
        return;

    while (_gui->isOpen())
    {
        manageEvent(_gui->getEvent());
        if (_reset)
        {
            initialize();
            setFrameIteration(1);
            _reset = false;
            render();
        }
        _gui->display();
        std::this_thread::sleep_for(std::chrono::milliseconds(8));
    }
}

/**
 * Renders the scene using the ray tracing algorithm.
 */
void Renderer::render(void)
{
    if (getScene() == nullptr || getCamera() == nullptr)
        return;
    if (_rayDirections.size() == 0)
        initialize();

    _renderStart = std::chrono::steady_clock::now();
    updateTitle();

    while (_frameIteration <= _frameIterations)
    {
        // Process GUI input only on the render thread, while no worker thread
        // is reading the camera.
        if (_gui)
        {
            manageEvent(_gui->getEvent());
            if (!_gui->isOpen())
            {
                _stop = true;
                break;
            }
            if (_reset)
            {
                initialize();
                setFrameIteration(1);
                _reset = false;
            }

            if (_paused)
            {
                _gui->display();
                std::this_thread::sleep_for(std::chrono::milliseconds(8));
                continue;
            }
        }

        std::clog << "\rRendering image. Percentage done: " << (float)(_frameIteration - 1) / _frameIterations * 100
                  << "% ";
        renderFrame();
        _frameIteration++;
        updateTitle();
        if (_stop)
            break;
    }
    if (_outStream)
        writePPM(*_outStream, getCamera()->getImageWidth(), getCamera()->getImageHeight(), _imageData);

    updateTitle();
    std::clog << "\rDone.                                             ";
    _rayDirections.clear();
}

/**
 * Renders a frame and accumulates the color data for sampling.
 */
void Renderer::renderFrame(void)
{
    const int width = getCamera()->getImageWidth();
    const int height = getCamera()->getImageHeight();
    std::size_t numThreads = std::max<std::size_t>(1, std::thread::hardware_concurrency());

    if (_threadCount > 0)
        numThreads = std::min(numThreads, _threadCount);

    numThreads = std::min<std::size_t>(numThreads, static_cast<std::size_t>(std::max(height, 1)));

    // Clear/re-initialize the image data if it's the first frame.
    if (_frameIteration == 1)
        std::fill(_imageAccumulationData.begin(), _imageAccumulationData.end(), Color4D());

    std::exception_ptr workerError;
    std::mutex workerErrorMutex;

    auto threadFunc = [&](int startY, int endY) {
        try
        {
            for (int y = startY; y < endY && !_reset && !_stop; ++y)
            {
                for (int x = 0; x < width && !_reset && !_stop; ++x)
                {
                    // Give every pixel its own seed so threads never share RNG state.
                    const uint32_t seed = Utils::PCGHash(static_cast<uint32_t>(x) +
                                                         0x9e3779b9u * static_cast<uint32_t>(y) +
                                                         static_cast<uint32_t>(_frameIteration) + _seed);
                    const std::size_t pixelIndex =
                        static_cast<std::size_t>(x) + static_cast<std::size_t>(y) * static_cast<std::size_t>(width);
                    Color4D pixelColor = getPixelColor(seed, x, y);

                    // Accumulate the color data for the pixel.
                    _imageAccumulationData[pixelIndex] += pixelColor;

                    // Set the pixel color to the average of the accumulated color data.
                    Color4D accumulatedColor4D = _imageAccumulationData[pixelIndex];
                    accumulatedColor4D /= (float)_frameIteration;
                    accumulatedColor4D.clampColor(0.0f, 1.0f);
                    _imageData[pixelIndex] = Utils::convertToRGBA(accumulatedColor4D, _postProcess);
                }
            }
        }
        catch (...)
        {
            std::lock_guard<std::mutex> lock(workerErrorMutex);

            if (!workerError)
                workerError = std::current_exception();
            _stop.store(true, std::memory_order_relaxed);
        }
    };

    const int partHeight = height / static_cast<int>(numThreads);
    std::vector<std::pair<int, int>> ranges;

    ranges.reserve(numThreads);

    for (std::size_t i = 0; i < numThreads; ++i)
    {
        const int startY = static_cast<int>(i) * partHeight;
        const int endY = (i == numThreads - 1) ? height : startY + partHeight;

        ranges.emplace_back(startY, endY);
    }

    if (!_pool)
        _pool = std::make_unique<RenderPool>(numThreads);
    else
        _pool->resize(numThreads);

    _pool->run(threadFunc, ranges);

    // Keep the window responsive while the workers trace the frame. Events are
    // queued rather than applied immediately: the camera must not be mutated
    // while worker threads are reading it. Any event interrupts the current
    // frame, then the queued events are applied after the workers finish.
    std::vector<char> pendingEvents;

    if (_gui)
    {
        while (_pool->running())
        {
            char event;

            while ((event = _gui->getEvent()) != 0)
            {
                pendingEvents.push_back(event);
                if (_cameraController.isCameraEvent(event))
                    _reset.store(true, std::memory_order_relaxed);
            }

            if (!_gui->isOpen())
                _stop.store(true, std::memory_order_relaxed);

            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
    }

    _pool->wait();

    if (workerError)
        std::rethrow_exception(workerError);

    if (_gui && _gui->isOpen())
    {
        for (const char event : pendingEvents)
            manageEvent(event);
    }

    if (_stop)
        return;
    if (_gui)
    {
        _gui->setImage(getCamera()->getImageWidth(), getCamera()->getImageHeight(), _imageData);
        _gui->display();
    }
}

void Renderer::refreshImage(void)
{
    const int completedSamples = _frameIteration - 1;

    if (completedSamples <= 0)
        return;

    for (std::size_t i = 0; i < _imageData.size(); ++i)
    {
        Color4D color = _imageAccumulationData[i];

        color /= static_cast<float>(completedSamples);
        _imageData[i] = Utils::convertToRGBA(color, _postProcess);
    }

    if (_gui)
    {
        _gui->setImage(getCamera()->getImageWidth(), getCamera()->getImageHeight(), _imageData);
        _gui->display();
    }
}

void Renderer::saveScreenshot(void)
{
    if (_imageData.empty())
        return;

    const auto stamp = std::chrono::duration_cast<std::chrono::milliseconds>(
                           std::chrono::system_clock::now().time_since_epoch())
                           .count();
    const std::string path = "raytracer-" + std::to_string(stamp) + ".ppm";

    writePPMFile(path, getCamera()->getImageWidth(), getCamera()->getImageHeight(), _imageData);
    std::clog << "\rSaved screenshot: " << path << std::endl;
}

void Renderer::updateTitle(void)
{
    if (!_gui)
        return;

    const int completedSamples = std::clamp(_frameIteration - 1, 0, _frameIterations);
    const double elapsed =
        std::chrono::duration<double>(std::chrono::steady_clock::now() - _renderStart).count();
    std::ostringstream title;

    title << "RayTracer | " << getCamera()->getImageWidth() << "x" << getCamera()->getImageHeight() << " | "
          << completedSamples << "/" << _frameIterations << " spp | " << std::fixed << std::setprecision(1) << elapsed
          << "s | exposure " << std::setprecision(2) << _postProcess.exposure;

    if (getCamera()->getLensRadius() > 0.0f)
        title << " | lens " << getCamera()->getLensRadius() << " | focus " << getCamera()->getFocusDistance();

    if (_paused)
        title << " | paused";

    _gui->setTitle(title.str());
}

void Renderer::manageEvent(char evt)
{
    if (!evt)
        return;

    if (evt == ' ')
    {
        _paused = !_paused;
        updateTitle();
        return;
    }

    if (evt == 'p' || evt == 'P')
    {
        saveScreenshot();
        return;
    }

    if (evt == '[' || evt == ']')
    {
        const float factor = evt == ']' ? 1.1f : 1.0f / 1.1f;

        _postProcess.exposure = std::clamp(_postProcess.exposure * factor, 0.05f, 20.0f);
        refreshImage();
        updateTitle();
        return;
    }

    if (_cameraController.handleEvent(evt, *getCamera()))
    {
        setFrameIteration(0);
        _reset = true;
        updateTitle();
    }
}

/**
 * Computes the direction of the next ray to be traced based on the hit material.
 *
 * @param ray The current ray being traced.
 * @param payload The hit payload containing information about the intersection.
 * @return The direction of the next ray to be traced.
 */
Vec3D Renderer::computeNextRayDirection(const Ray &ray, const HitPayload &payload, uint32_t &seed,
                                         Color3D &throughput, const Color3D &surfaceColor)
{
    Vec3D rayDirection;
    const auto &material =
        getScene()->getMaterials().at(getScene()->getPrimitives()[payload.getHitIndex()]->getMaterialIndex());

    switch (material->getType())
    {
    case IMaterial::DIFFUSE:
        // Cosine-weighted sampling is the correct distribution for a
        // Lambertian surface and reduces noise compared with uniform sampling.
        rayDirection = vec3DRandomCosineHemisphere(payload.getShadingNormal(), seed);
        break;
    case IMaterial::METALLIC:
    {
        const Vec3D viewDirection = vec3DNormalize(ray.getOrigin() - payload.getHitPosition());
        Color3D weight;

        if (!sampleMetallic(viewDirection, payload.getShadingNormal(), surfaceColor, material->getRoughness(), seed,
                            rayDirection, weight))
            return Vec3D(0.0f);

        throughput *= weight;
        break;
    }
    case IMaterial::EMISSIVE:   // Return 0 light (it should have stoped befor this funcion in an if condition)
    case IMaterial::FLAT_COLOR: // (Does not bounce)
    case IMaterial::DIELECTRIC: // Handled separately in getPixelColor.
    case IMaterial::TYPE_COUNT: // None
    default:
        break;
    }
    return rayDirection;
}

/**
 * Computes the Phong contribution of one light sample.
 *
 * Ambient samples only contribute their radiance. Directional and area samples
 * combine diffuse and specular terms using the sampled direction and radiance.
 *
 * @param payload The hit payload containing information about the intersection point.
 * @param material The material of the hit primitive.
 * @param sample The sampled light connection.
 * @param ambient Whether the light is ambient.
 * @param ray The current camera or bounce ray.
 * @return The computed colour contribution.
 */
Color3D Renderer::computeLightContribution(const HitPayload &payload, const IMaterial &material,
                                           const LightSample &sample, bool ambient, const Ray &ray)
{
    const float reflectionCoefficient = material.getAmbientReflectionCoefficient() +
                                        material.getDiffuseReflectionCoefficient() +
                                        material.getSpecularReflectionCoefficient();

    if (ambient)
        return reflectionCoefficient * sample.radiance;

    const Vec3D normal = payload.getShadingNormal();
    const float diffuseFactor = std::max(vec3DDot(normal, sample.direction), 0.0f);

    if (diffuseFactor <= 0.0f)
        return Color3D(0.0f);

    const Vec3D viewDirection = vec3DNormalize(ray.getOrigin() - payload.getHitPosition());
    const Vec3D halfDirection = vec3DNormalize(sample.direction + viewDirection);
    const float shininess = blinnShininess(material.getRoughness());
    const float specularFactor = std::pow(std::max(vec3DDot(normal, halfDirection), 0.0f), shininess);

    return reflectionCoefficient * sample.radiance * diffuseFactor +
           material.getSpecularReflectionCoefficient() * sample.radiance * specularFactor;
}

/**
 * Retrieves the color of a pixel at the specified coordinates.
 *
 * Main raytracing algorithm.
 *
 * @param x The x-coordinate of the pixel (from the image loop).
 * @param y The y-coordinate of the pixel (from the image loop).
 * @return The color of the pixel.
 */
Color4D Renderer::getPixelColor(uint32_t seed, int x, int y)
{
    Ray ray;
    int bounces = 15;
    Color3D accumulatedColor(0.0f);
    Color3D throughput(1.0f);
    uint32_t sampler = seed;
    bool insideDielectric = false;
    // float brightReduction = 1.5;

    const Camera &camera = *getCamera();
    Scene &scene = *getScene();
    const Primitives &primitives = scene.getPrimitives();
    const Materials &materials = scene.getMaterials();
    const Lights &lights = scene.getLights();
    const std::size_t pixelIndex =
        static_cast<std::size_t>(x) + static_cast<std::size_t>(y) * static_cast<std::size_t>(camera.getImageWidth());

    // Set the origin and direction of the ray. The jitter is expressed in
    // pixel units so anti-aliasing is independent of scene scale and distance.
    Vec3D direction = _rayDirections[pixelIndex];
    const float pixelRandomPosX = Utils::numericGeneratePseudoRandom<float>(sampler, -0.5f, 0.5f);
    const float pixelRandomPosY = Utils::numericGeneratePseudoRandom<float>(sampler, -0.5f, 0.5f);

    direction += pixelRandomPosX * camera.getPixelWidthDistance() +
                 pixelRandomPosY * camera.getPixelHeightDistance();

    Vec3D origin = camera.getLookFrom();
    const float lensRadius = camera.getLensRadius();

    // A zero lens radius keeps the pinhole camera and must not consume random
    // values, so existing scenes stay byte-identical.
    if (lensRadius > 0.0f && camera.getFocalDistance() > 0.0)
    {
        const float lensU = Utils::numericGeneratePseudoRandom<float>(sampler, 0.0f, 1.0f);
        const float lensV = Utils::numericGeneratePseudoRandom<float>(sampler, 0.0f, 1.0f);
        const float lensDistance = std::sqrt(lensU);
        const float lensAngle = 2.0f * PI_F * lensV;

        origin += camera.getCamViewRight() * (lensDistance * std::cos(lensAngle) * lensRadius) +
                  camera.getCamViewUp() * (lensDistance * std::sin(lensAngle) * lensRadius);

        const float focusRatio = camera.getFocusDistance() / static_cast<float>(camera.getFocalDistance());

        direction = (camera.getLookFrom() + direction * focusRatio) - origin;
    }

    ray.setOrigin(origin);
    ray.setDirection(direction);

    // Shoot the ray, bounce it around the scene, and accumulate the color of the pixel
    for (int bounce = 0; bounce < bounces; ++bounce)
    {
        HitPayload payload = shotRay(ray);

        // If we hit an object
        if (payload.getHitDistance() != std::numeric_limits<float>::infinity())
        {
            const auto &primitive = primitives[payload.getHitIndex()];
            const int materialOverride = payload.getMaterialIndex();
            const uint materialIndex =
                materialOverride >= 0 && static_cast<std::size_t>(materialOverride) < materials.size()
                    ? static_cast<uint>(materialOverride)
                    : primitive->getMaterialIndex();
            const auto &material = materials.at(materialIndex);
            const IMaterial::MaterialType materialType = material->getType();
            Color3D surfaceColor = material->getDiffuseColor();

            if (const std::shared_ptr<const Texture> texture = material->getTexture())
                surfaceColor *= texture->sample(payload.getU(), payload.getV());

            if (materialType == IMaterial::DIELECTRIC)
            {
                const Vec3D normal = payload.getHitNormal();
                const float cosi = std::clamp(-vec3DDot(ray.getDirection(), normal), 0.0f, 1.0f);
                const float ior = material->getIndexOfRefraction();
                const float etaI = insideDielectric ? ior : 1.0f;
                const float etaT = insideDielectric ? 1.0f : ior;
                const float eta = etaI / etaT;
                const float k = 1.0f - eta * eta * (1.0f - cosi * cosi);
                const float ratio = (etaI - etaT) / (etaI + etaT);
                const float reflectance = ratio * ratio;
                const float fresnel = reflectance + (1.0f - reflectance) * std::pow(1.0f - cosi, 5.0f);
                const bool reflectRay =
                    k < 0.0f || Utils::numericGeneratePseudoRandom<float>(sampler, 0.0f, 1.0f) < fresnel;
                Vec3D nextDirection;

                if (reflectRay)
                    nextDirection = vec3DNormalize(reflect(ray.getDirection(), normal));
                else
                {
                    nextDirection =
                        vec3DNormalize(eta * ray.getDirection() + (eta * cosi - std::sqrt(k)) * normal);
                    insideDielectric = !insideDielectric;
                }

                if (vec3DIsNearZero(nextDirection))
                    break;

                throughput *= surfaceColor;

                if (throughput.lengthSquared() < 1e-6f)
                    break;

                ray.setDirection(nextDirection);
                ray.setOrigin(payload.getHitPosition() + nextDirection * Ray::EPSILON);
                continue;
            }

            // If the material emits light, add its emission to the accumulated color
            if (materialType == IMaterial::EMISSIVE)
                accumulatedColor += throughput * material->getEmissionColor();

            // Compute lighting contributions from each light source
            for (size_t i = 0; i < lights.size(); ++i)
            {
                const LightSample lightSample = lights[i]->sample(payload, scene, sampler);

                if (!lightSample.valid)
                    continue;

                // Calculate Phong lighting model for the hit point and the current light
                Color3D phongLighting =
                    computeLightContribution(payload, *material, lightSample, lights[i]->isAmbient(), ray);

                // Reduce the Phong lighting because it's too bright
                // phongLighting /= brightReduction;

                // Multiply the Phong components by the object's color (accumulate object color)
                Color3D phongColor = phongLighting * surfaceColor;

                // Accumulate the Phong color, attenuated by the path throughput.
                accumulatedColor += throughput * phongColor;
            }

            // Emissive and flat materials do not bounce. Dielectrics were
            // handled above and never reach this point.
            if (materialType == IMaterial::EMISSIVE || materialType == IMaterial::FLAT_COLOR)
                break;

            // Diffuse paths are attenuated by the surface colour. Metallic
            // paths apply their Fresnel/GGX weight inside the sampler.
            if (materialType == IMaterial::DIFFUSE)
            {
                throughput *= surfaceColor;

                if (throughput.lengthSquared() < 1e-6f)
                    break;
            }

            // Handle set direction and origin for the next ray
            const Vec3D nextDirection = computeNextRayDirection(ray, payload, sampler, throughput, surfaceColor);

            if (throughput.lengthSquared() < 1e-6f)
                break;

            // A zero or inward-facing direction would trace a degenerate ray
            // forever, so stop instead.
            if (vec3DIsNearZero(nextDirection) || vec3DDot(nextDirection, payload.getHitNormal()) <= 0.0f)
                break;

            ray.setDirection(nextDirection);
            ray.setOrigin(payload.getHitPosition() + payload.getHitNormal() * Ray::EPSILON);
        }
        else
        {
            const Background &background = scene.getBackground();

            if (background.enabled)
                accumulatedColor += throughput * background.sample(ray.getDirection());
            break;
        }
    }
    return Color4D(accumulatedColor, 1);
}

/**
 * Shoots a ray into the scene and returns the hit payload.
 *
 * @param ray The ray to be shot into the scene.
 * @return The hit payload containing information about the intersection.
 */
HitPayload Renderer::shotRay(const Ray &ray)
{
    HitPayload payload;
    payload.setHitDistance(std::numeric_limits<float>::infinity());

    // Handle no objects
    if (getScene()->getPrimitives().size() == 0)
        return missHit();

    //! In primitive hit, IGNORE NEGATIVE DISTANCES.
    if (getScene()->hit(ray, payload, 0))
        return payload;
    return missHit();
}

/**
 * This function is called when a ray misses all the objects in the scene.
 * It returns a HitPayload object representing the result of the miss hit.
 *
 * @return The HitPayload object representing the result of the miss hit.
 */
HitPayload Renderer::missHit(void)
{
    HitPayload payload;
    payload.setHitDistance(std::numeric_limits<float>::infinity());
    return payload;
}

void Renderer::setScene(std::shared_ptr<Scene> scene)
{
    _scene = scene;
}

void Renderer::setCamera(std::shared_ptr<Camera> camera)
{
    _camera = std::move(camera);
}

void Renderer::setFrameIterations(int frameIterations)
{
    if (frameIterations <= 0)
        throw std::invalid_argument("Frame iterations must be positive");

    _frameIterations = frameIterations;
    _frameIterationsScale = 1.0f / static_cast<float>(frameIterations);
}

void Renderer::setFrameIterationsScale(float frameIterationsScale)
{
    _frameIterationsScale = frameIterationsScale;
}

void Renderer::setFrameIteration(int frameIteration)
{
    _frameIteration = frameIteration;
}

int Renderer::getFrameIterations(void) const
{
    return _frameIterations;
}

float Renderer::getFrameIterationsScale(void) const
{
    return _frameIterationsScale;
}

std::shared_ptr<Scene> Renderer::getScene(void) const
{
    return _scene;
}

std::shared_ptr<Camera> Renderer::getCamera(void) const
{
    return _camera;
}

int Renderer::getFrameIteration(void) const
{
    return _frameIteration;
}
