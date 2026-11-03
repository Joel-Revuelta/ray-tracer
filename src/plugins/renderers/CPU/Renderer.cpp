/*
** EPITECH PROJECT, 2023
** raytracer
** File description:
** Renderer.cpp
*/

#include "plugins/renderers/CPU/Renderer.hpp"
#include "Utils.hpp"
#include "plugins/renderers/PPM.hpp"
#include <algorithm>
#include <chrono>
#include <cmath>
#include <condition_variable>
#include <cstdint>
#include <exception>
#include <functional>
#include <mutex>
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
      _gui(nullptr)
{
}

Renderer::Renderer(int raysPerPixel, std::shared_ptr<Scene> scene, std::shared_ptr<Camera> camera,
                   std::shared_ptr<IGUI> gui, const std::string &outFile)
    : _frameIterations(raysPerPixel), _frameIterationsScale(1.0 / _frameIterations), _scene(scene), _camera(camera),
      _gui(gui)
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
        }

        std::clog << "\rRendering image. Percentage done: " << (float)(_frameIteration - 1) / _frameIterations * 100
                  << "% ";
        renderFrame();
        _frameIteration++;
        if (_stop)
            break;
    }
    if (_outStream)
        writePPM(*_outStream, getCamera()->getImageWidth(), getCamera()->getImageHeight(), _imageData);

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
                                                         static_cast<uint32_t>(_frameIteration));
                    const std::size_t pixelIndex =
                        static_cast<std::size_t>(x) + static_cast<std::size_t>(y) * static_cast<std::size_t>(width);
                    Color4D pixelColor = getPixelColor(seed, x, y);

                    // Accumulate the color data for the pixel.
                    _imageAccumulationData[pixelIndex] += pixelColor;

                    // Set the pixel color to the average of the accumulated color data.
                    Color4D accumulatedColor4D = _imageAccumulationData[pixelIndex];
                    accumulatedColor4D /= (float)_frameIteration;
                    accumulatedColor4D.clampColor(0.0f, 1.0f);
                    _imageData[pixelIndex] = Utils::convertToRGBA(accumulatedColor4D);
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

void Renderer::manageEvent(char evt)
{
    if (_cameraController.handleEvent(evt, *getCamera()))
    {
        setFrameIteration(0);
        _reset = true;
    }
}

/**
 * Computes the direction of the next ray to be traced based on the hit material.
 *
 * @param ray The current ray being traced.
 * @param payload The hit payload containing information about the intersection.
 * @return The direction of the next ray to be traced.
 */
Vec3D Renderer::computeNextRayDirection(const Ray &ray, const HitPayload &payload, uint32_t &seed)
{
    Vec3D rayDirection;
    const auto &material =
        getScene()->getMaterials().at(getScene()->getPrimitives()[payload.getHitIndex()]->getMaterialIndex());

    switch (material->getType())
    {
    case IMaterial::DIFFUSE:
        // Cosine-weighted sampling is the correct distribution for a
        // Lambertian surface and reduces noise compared with uniform sampling.
        rayDirection = vec3DRandomCosineHemisphere(payload.getHitNormal(), seed);
        break;
    case IMaterial::METALLIC:
        // Reflect the ray direction about the hit normal
        rayDirection = vec3DNormalize(reflect(ray.getDirection(), payload.getHitNormal()));
        break;
    case IMaterial::EMISSIVE:   // Return 0 light (it should have stoped befor this funcion in an if condition)
    case IMaterial::FLAT_COLOR: // (Does not bounce)
    case IMaterial::DIELECTRIC: // Not implemented
    case IMaterial::TYPE_COUNT: // None
    default:
        break;
    }
    return rayDirection;
}

/**
 * Computes the Phong lighting model for a given hit payload and light source.
 *
 * This method calculates the Phong lighting model for a given hit payload and light source.
 * The Phong lighting model combines ambient, diffuse, and specular lighting components to
 * simulate the interaction of light with a surface.
 *
 * @param payload The hit payload containing information about the intersection point.
 * @param light The light source used for computing the lighting.
 * @return The computed color using the Phong lighting model.
 */
Color3D Renderer::computePhongLighting(const HitPayload &payload, const IMaterial &material,
                                       const std::shared_ptr<ILight> &light, const Ray &ray)
{
    const Color3D radiance = light->getRadiance(payload);
    const float reflectionCoefficient = material.getAmbientReflectionCoefficient() +
                                        material.getDiffuseReflectionCoefficient() +
                                        material.getSpecularReflectionCoefficient();

    if (light->isAmbient())
        return reflectionCoefficient * radiance;

    const Vec3D normal = payload.getHitNormal();
    const Vec3D lightDirection = light->getLightDirection(payload);
    const float diffuseFactor = std::max(vec3DDot(normal, lightDirection), 0.0f);

    if (diffuseFactor <= 0.0f)
        return Color3D(0.0f);

    const Vec3D viewDirection = vec3DNormalize(ray.getOrigin() - payload.getHitPosition());
    const Vec3D halfDirection = vec3DNormalize(lightDirection + viewDirection);
    constexpr float shininess = 64.0f;
    const float specularFactor = std::pow(std::max(vec3DDot(normal, halfDirection), 0.0f), shininess);

    return reflectionCoefficient * radiance * diffuseFactor +
           material.getSpecularReflectionCoefficient() * radiance * specularFactor;
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
    ray.setOrigin(camera.getLookFrom());
    Vec3D direction = _rayDirections[pixelIndex];
    const float pixelRandomPosX = Utils::numericGeneratePseudoRandom<float>(sampler, -0.5f, 0.5f);
    const float pixelRandomPosY = Utils::numericGeneratePseudoRandom<float>(sampler, -0.5f, 0.5f);

    direction += pixelRandomPosX * camera.getPixelWidthDistance() +
                 pixelRandomPosY * camera.getPixelHeightDistance();
    ray.setDirection(direction);

    // Shoot the ray, bounce it around the scene, and accumulate the color of the pixel
    for (int bounce = 0; bounce < bounces; ++bounce)
    {
        HitPayload payload = shotRay(ray);

        // If we hit an object
        if (payload.getHitDistance() != std::numeric_limits<float>::infinity())
        {
            const auto &primitive = primitives[payload.getHitIndex()];
            const auto &material = materials.at(primitive->getMaterialIndex());
            const IMaterial::MaterialType materialType = material->getType();

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

                throughput *= material->getDiffuseColor();

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
                // Cast shadow rays to determine shadowness (cast with an offset to avoid self-intersection)
                if (lights[i]->isPointInShadow(payload, scene))
                    continue;

                // Calculate Phong lighting model for the hit point and the current light
                Color3D phongLighting = computePhongLighting(payload, *material, lights[i], ray);

                // Reduce the Phong lighting because it's too bright
                // phongLighting /= brightReduction;

                // Multiply the Phong components by the object's color (accumulate object color)
                Color3D phongColor = phongLighting * material->getDiffuseColor();

                // Accumulate the Phong color, attenuated by the path throughput.
                accumulatedColor += throughput * phongColor;
            }

            // Emissive and flat materials do not bounce. Dielectrics were
            // handled above and never reach this point.
            if (materialType == IMaterial::EMISSIVE || materialType == IMaterial::FLAT_COLOR)
                break;

            // Diffuse and metallic paths are attenuated by the surface colour.
            if (materialType == IMaterial::DIFFUSE || materialType == IMaterial::METALLIC)
            {
                throughput *= material->getDiffuseColor();

                if (throughput.lengthSquared() < 1e-6f)
                    break;
            }

            // Handle set direction and origin for the next ray
            const Vec3D nextDirection = computeNextRayDirection(ray, payload, sampler);

            // A zero or inward-facing direction would trace a degenerate ray
            // forever, so stop instead.
            if (vec3DIsNearZero(nextDirection) || vec3DDot(nextDirection, payload.getHitNormal()) <= 0.0f)
                break;

            ray.setDirection(nextDirection);
            ray.setOrigin(payload.getHitPosition() + payload.getHitNormal() * Ray::EPSILON);
        }
        else
        {
            // If we didn't hit anything, return the background color
            accumulatedColor += Color3D(0.0, 0.0, 0.0);
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
