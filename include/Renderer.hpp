/*
** EPITECH PROJECT, 2023
** raytracer
** File description:
** Renderer.hpp
*/

#ifndef RENDERER_HPP_
#define RENDERER_HPP_

#include "Camera.hpp"
#include "Color.hpp"
#include "GUI.hpp"
#include "HitPayload.hpp"
#include "Light.hpp"
#include "Primitive.hpp"
#include "Ray.hpp"
#include "Scene.hpp"
#include "Vec3D.hpp"
#include <cstddef>
#include <iostream>
#include <memory>
#include <mutex>
#include <sys/types.h>
#include <unordered_map>
#include <vector>
#include <fstream>

namespace RayTracer
{

class Renderer
{
  public:
    Renderer(void);
    Renderer(int frameIterations, std::shared_ptr<Scene> scene, std::shared_ptr<Camera> camera,
             std::shared_ptr<IGUI> gui, const std::string& outFile);
    ~Renderer(void);

    void run(void);
    void render(void);

    void setFrameIteration(int frameIteration);
    void setFrameIterations(int frameIterations);
    void setFrameIterationsScale(float frameIterationsScale);
    void setScene(std::shared_ptr<Scene> scene);
    void setCamera(std::shared_ptr<Camera> camera);

    int getFrameIteration(void) const;
    int getFrameIterations(void) const;
    float getFrameIterationsScale(void) const;
    std::shared_ptr<Scene> getScene(void) const;
    std::shared_ptr<Camera> getCamera(void) const;

  private:
    // How many rays to cast per pixel
    int _frameIterations;

    // Frame iteration
    int _frameIteration = 1;

    // Scale to factor color values to [0, 1]
    float _frameIterationsScale;
    // Vector of starting ray directions
    std::vector<Vec3D> _rayDirections;

    // Image data storage
    std::vector<uint32_t> _imageData;

    // Image accumulation data storage
    std::vector<Color4D> _imageAccumulationData;

    // Scene and camera
    std::shared_ptr<Scene> _scene;
    std::shared_ptr<Camera> _camera;

    std::shared_ptr<IGUI> _gui;

    std::mutex _mutexEvents;
    bool _reset = false;
    bool _stop = false;

    std::ostream *_outStream;
    std::fstream _fileStream;

    // Private methods
  private:
    void initialize(void);

    // Raytracing methods
    void renderFrame(void);
    Color4D getPixelColor(uint32_t &seed, int x, int y);
    Color3D computePhongLighting(const HitPayload &payload, const std::shared_ptr<ILight> &light);
    Vec3D computeNextRayDirection(const Ray &ray, const HitPayload &payload);
    HitPayload shotRay(const Ray &ray);
    HitPayload missHit(void);
    void manageEvent(char evt);
};

} // namespace RayTracer

#endif // RENDERER_HPP_
