/*
** EPITECH PROJECT, 2023
** raytracer
** File description:
** Renderer.cpp
*/

#include "plugins/renderers/CPU/Renderer.hpp"
#include "render/AtmosphereSampling.hpp"
#include "render/DielectricStack.hpp"
#include "render/ImageWriter.hpp"
#include "render/Tangent.hpp"
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
    : _frameIterations(raysPerPixel), _frameIterationsScale(1.0f), _scene(scene), _camera(camera), _gui(gui),
      _postProcess(postProcess), _threadCount(threads), _seed(seed)
{
    if (raysPerPixel <= 0)
        throw std::invalid_argument("Frame iterations must be positive");

    _frameIterationsScale = 1.0f / static_cast<float>(_frameIterations);

    if (!outFile.empty())
        _outputPath = outFile;
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
    _rayDirections.reserve(pixelCount);
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
    if (!_outputPath.empty())
        writeImageFileAtomic(_outputPath, getCamera()->getImageWidth(), getCamera()->getImageHeight(), _imageData);
    else if (_outStream)
        writePPM(*_outStream, getCamera()->getImageWidth(), getCamera()->getImageHeight(), _imageData);

    updateTitle();
    std::clog << "\rDone.                                             ";
    _rayDirections.clear();
}

/**
 * Renders a frame and accumulates the color data for sampling.
 */
void Renderer::renderFrame(bool present)
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

                    // Accumulate the color data for the pixel. The final HDR
                    // average is post-processed once all workers finish.
                    _imageAccumulationData[pixelIndex] += pixelColor;
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

    if (!present)
        return;

    updateImage(_frameIteration);

    if (_gui)
    {
        _gui->setImage(getCamera()->getImageWidth(), getCamera()->getImageHeight(), _imageData);
        _gui->display();
    }
}

void Renderer::updateImage(int completedSamples)
{
    if (completedSamples <= 0 || !getCamera())
        return;

    const int width = getCamera()->getImageWidth();
    const int height = getCamera()->getImageHeight();
    const std::size_t count = static_cast<std::size_t>(width) * static_cast<std::size_t>(height);

    if (_imageAccumulationData.size() < count || _imageData.size() < count)
        return;

    std::vector<Color3D> hdr(count);
    const float inverseSamples = 1.0f / static_cast<float>(completedSamples);

    for (std::size_t i = 0; i < count; ++i)
    {
        const Color4D &accumulated = _imageAccumulationData[i];

        hdr[i] = Color3D(accumulated.getX(), accumulated.getY(), accumulated.getZ()) * inverseSamples;
    }

    _imageData = postProcessToRGBA(std::move(hdr), width, height, _postProcess);
}

std::vector<Color3D> Renderer::getHDRImage(void) const
{
    if (!getCamera() || _frameIterations <= 0)
        return {};

    const int width = getCamera()->getImageWidth();
    const int height = getCamera()->getImageHeight();
    const std::size_t count = static_cast<std::size_t>(width) * static_cast<std::size_t>(height);

    if (_imageAccumulationData.size() < count)
        return {};

    std::vector<Color3D> hdr(count);
    const float inverseSamples = 1.0f / static_cast<float>(_frameIterations);

    for (std::size_t i = 0; i < count; ++i)
    {
        const Color4D &accumulated = _imageAccumulationData[i];

        hdr[i] = Color3D(accumulated.getX(), accumulated.getY(), accumulated.getZ()) * inverseSamples;
    }
    return hdr;
}

std::vector<Color3D> Renderer::renderHDR(void)
{
    _stop.store(false);
    _reset.store(false);
    initialize();
    setFrameIteration(1);

    while (_frameIteration <= _frameIterations)
    {
        renderFrame(false);
        _frameIteration++;
        if (_stop)
            break;
    }
    return getHDRImage();
}

void Renderer::refreshImage(void)
{
    const int completedSamples = _frameIteration - 1;

    if (completedSamples <= 0)
        return;

    updateImage(completedSamples);

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

    writeImageFileAtomic(path, getCamera()->getImageWidth(), getCamera()->getImageHeight(), _imageData);
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

void Renderer::setScene(std::shared_ptr<Scene> scene)
{
    _scene = scene;
}

void Renderer::setCamera(std::shared_ptr<Camera> camera)
{
    _camera = std::move(camera);
}

void Renderer::setSeed(uint32_t seed)
{
    _seed = seed;
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
