/*
** EPITECH PROJECT, 2023
** raytracer
** File description:
** Renderer.cpp
*/

#include "Renderer.hpp"
#include "Utils.hpp"
#include <thread>

using namespace RayTracer;

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
    if (outFile.empty())
        _outStream = &std::cout;
    else {
        _fileStream.open(outFile, std::ios::out);
        _outStream = &_fileStream;
    }

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
    int imageWidth = getCamera()->getImageWidth();
    int imageHeight = getCamera()->getImageHeight();

    // Initialize the imageData to 0.
    _imageData.clear();
    _imageData.resize(imageWidth * imageHeight);

    // Initialize the _imageAccumulationData to basic Color4D.
    _imageAccumulationData.clear();
    _imageAccumulationData.resize(imageWidth * imageHeight);

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
    render();
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
    }
    std::clog << std::endl;
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
        std::clog << "\rRendering image. Percentage done: " << (float)(_frameIteration - 1) / _frameIterations * 100
                  << "% ";
        renderFrame();
        _frameIteration++;
        if (_stop)
            break;
    }
    *_outStream << "P3" << std::endl << getCamera()->getImageWidth() << " " << getCamera()->getImageHeight() << "\n255" << std::endl;
    for (size_t i = 0; i < _imageData.size(); ++i)
        *_outStream << Utils::convertTo255(Utils::convertFromRGBA(_imageData[i])) << std::endl;
    std::clog << "\rDone.                                             ";
    _rayDirections.clear();
}

/**
 * Renders a frame and accumulates the color data for sampling.
 */
void Renderer::renderFrame(void)
{
    std::size_t numThreads = std::thread::hardware_concurrency();
    std::vector<std::thread> threads(numThreads);

    uint32_t seed = 999;

    // Clear/re-initialize the image data if it's the first frame.
    if (_frameIteration == 1)
        std::fill(_imageAccumulationData.begin(), _imageAccumulationData.end(), Color4D());

    auto threadFunc = [&](int startY, int endY) {
        for (int y = startY; y < endY && !_reset && !_stop; ++y)
        {
            for (int x = 0; x < getCamera()->getImageWidth() && !_reset && !_stop; ++x)
            {
                // Get the color data for the pixel.
                Color4D pixelColor = getPixelColor(seed, x, y);

                // Accumulate the color data for the pixel.
                _imageAccumulationData[x + y * getCamera()->getImageWidth()] += pixelColor;

                // Set the pixel color to the average of the accumulated color data.
                Color4D accumulatedColor4D = _imageAccumulationData[x + y * getCamera()->getImageWidth()];
                accumulatedColor4D /= (float)_frameIteration;
                accumulatedColor4D.clampColor(0.0f, 1.0f);
                _imageData[x + y * getCamera()->getImageWidth()] = Utils::convertToRGBA(accumulatedColor4D);
                if (_mutexEvents.try_lock())
                {
                    manageEvent(_gui->getEvent());
                    if (!_gui->isOpen())
                    {
                        _stop = true;
                        _mutexEvents.unlock();
                        return;
                    }
                    _mutexEvents.unlock();
                }
            }
        }
    };

    int height = getCamera()->getImageHeight();
    int partHeight = height / numThreads;

    for (std::size_t i = 0; i < numThreads; ++i)
    {
        int startY = i * partHeight;
        int endY = (i == numThreads - 1) ? height : startY + partHeight;
        threads[i] = std::thread(threadFunc, startY, endY);
    }

    for (auto &thread : threads)
    {
        if (thread.joinable())
            thread.join();
    }

    if (_stop)
        return;
    if (_reset)
    {
        initialize();
        _reset = false;
    }
    _gui->setImage(getCamera()->getImageWidth(), getCamera()->getImageHeight(), _imageData);
    _gui->display();
}

void Renderer::manageEvent(char evt)
{
    if (!evt)
        return;

    static bool lockLookAt = false;
    static bool lockLookFrom = false;
    // key, lookFrom, lookAt
    const std::unordered_map<char, std::pair<Vec3D, Vec3D>> keyMap = {
        {'w', {Vec3D(0, 0, 1), Vec3D(0, 0, 1)}}, {'s', {Vec3D(0, 0, -1), Vec3D(0, 0, -1)}},
        {'a', {Vec3D(1, 0, 0), Vec3D(1, 0, 0)}}, {'d', {Vec3D(-1, 0, 0), Vec3D(-1, 0, 0)}},
        {'q', {Vec3D(0, 1, 0), Vec3D(0, 1, 0)}}, {'e', {Vec3D(0, -1, 0), Vec3D(0, -1, 0)}},
        {'r', {Vec3D(0, 0, 0), Vec3D(0, 0, 0)}},
    };

    if (keyMap.find(evt) != keyMap.end())
    {
        auto move = keyMap.at(evt);
        auto newLookFrom = lockLookFrom ? getCamera()->getLookFrom() : getCamera()->getLookFrom() + move.first;
        auto newLookAt = lockLookAt ? getCamera()->getLookAt() : getCamera()->getLookAt() + move.second;
        getCamera()->initialize(getCamera()->getImageWidth(), getCamera()->getImageHeight(), newLookAt, newLookFrom,
                                getCamera()->getVerticalFovDegree());
        setFrameIteration(0);
        _reset = true;
    }
    if (evt == 'x')
        lockLookAt = !lockLookAt;
    if (evt == 'c')
        lockLookFrom = !lockLookFrom;
}

/**
 * Computes the direction of the next ray to be traced based on the hit material.
 *
 * @param ray The current ray being traced.
 * @param payload The hit payload containing information about the intersection.
 * @return The direction of the next ray to be traced.
 */
Vec3D Renderer::computeNextRayDirection(const Ray &ray, const HitPayload &payload)
{
    Vec3D rayDirection;
    const std::shared_ptr<IMaterial> material =
        getScene()->getMaterials().at(getScene()->getPrimitives()[payload.getHitIndex()]->getMaterialIndex());

    switch (material->getType())
    {
    case IMaterial::DIFFUSE:
        // Random direction on the hemisphere of the hit normal (better diffuse lighting)
        rayDirection = vec3DRandomOnHemisphere(payload.getHitNormal());
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
Color3D Renderer::computePhongLighting(const HitPayload &payload, const std::shared_ptr<ILight> &light)
{
    const std::shared_ptr<IMaterial> material =
        getScene()->getMaterials().at(getScene()->getPrimitives()[payload.getHitIndex()]->getMaterialIndex());
    Vec3D ambient(0);
    Vec3D diffuse(0);
    Vec3D specular(0);

    // Compute lighting contribution from the given light source
    Vec3D lightContributions = light->calculateLighting(payload);

    // Compute Phong lighting components
    ambient = material->getAmbientReflectionCoefficient() * lightContributions;
    diffuse = material->getDiffuseReflectionCoefficient() * lightContributions;
    specular = material->getSpecularReflectionCoefficient() * lightContributions;

    // Return the combined Phong lighting
    return ambient + diffuse + specular;
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
Color4D Renderer::getPixelColor(uint32_t &seed, int x, int y)
{
    Ray ray;
    int bounces = 15;
    Vec3D nextDirection(0.0f);
    Color3D accumulatedColor(0.0f);
    // float brightReduction = 1.5;

    // Set the origin and direction of the ray
    ray.setOrigin(getCamera()->getLookFrom());
    ray.setDirection(_rayDirections[x + y * getCamera()->getImageWidth()]);

    // Random operations to generate a new seed.
    seed = seed * x * y;

    // Anti-aliasing
    float pixelRandomPosX = Utils::numericGeneratePseudoRandom<float>(seed, 0.0, 0.03);
    float pixelRandomPosY = Utils::numericGeneratePseudoRandom<float>(seed, 0.0, 0.03);
    ray.setDirection(Vec3D(ray.getDirection().getX() + pixelRandomPosX, ray.getDirection().getY() + pixelRandomPosY,
                           ray.getDirection().getZ()));

    // Shoot the ray, bounce it around the scene, and accumulate the color of the pixel
    for (int bounce = 0; bounce < bounces; ++bounce)
    {
        HitPayload payload = shotRay(ray);

        // If we hit an object
        if (payload.getHitDistance() != std::numeric_limits<float>::infinity())
        {
            const Lights lights = getScene()->getLights();
            const std::shared_ptr<IPrimitive> primitive = getScene()->getPrimitives()[payload.getHitIndex()];
            const std::shared_ptr<IMaterial> material = getScene()->getMaterials().at(primitive->getMaterialIndex());

            // If the material emits light, add its emission to the accumulated color
            if (material->getType() == IMaterial::EMISSIVE)
                accumulatedColor += material->getEmissionColor();

            // Compute lighting contributions from each light source
            for (size_t i = 0; i < getScene()->getLights().size(); ++i)
            {
                // Cast shadow rays to determine shadowness (cast with an offset to avoid self-intersection)
                if (lights[i]->isPointInShadow(payload, *getScene()))
                    continue;

                // Calculate Phong lighting model for the hit point and the current light
                Color3D phongLighting = computePhongLighting(payload, lights[i]);

                // Reduce the Phong lighting because it's too bright
                // phongLighting /= brightReduction;

                // Multiply the Phong components by the object's color (accumulate object color)
                Color3D phongColor = phongLighting * material->getDiffuseColor();

                // Accumulate the Phong color
                accumulatedColor += phongColor;
            }

            // If the material is emissive, break the loop (it does not bounce)
            if (material->getType() == IMaterial::EMISSIVE)
                break;

            // Handle set direction and origin for the next ray
            ray.setDirection(computeNextRayDirection(ray, payload));
            ray.setOrigin(payload.getHitPosition() + ray.getDirection() * Ray::EPSILON);
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
    _frameIterations = frameIterations;
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
