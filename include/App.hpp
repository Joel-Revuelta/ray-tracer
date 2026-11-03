/*
** EPITECH PROJECT, 2023
** raytracer
** File description:
** App.hpp
*/

#ifndef APP_HPP_
#define APP_HPP_

#include "Factory.hpp"
#include "Parser.hpp"
#include "render/PostProcess.hpp"
#include <cstdint>
#include <getopt.h>
#include <memory>
#include <string>

enum Interface
{
    NONE = -1,
    GUI = 0,
    CLI = 1
};

namespace RayTracer
{

class App
{
  public:
    App(int ac, char **av);
    ~App(void);
    void run(void);

  private:
    std::unique_ptr<Parser> _parser;
    std::string _filepath;
    std::string _output;
    std::string _rendererName;
    std::string _guiName;
    PostProcessSettings _postProcess;
    Interface _interface;
    int _width;
    int _height;
    int _frames;
    int _threads = 0;
    int _device = 0;
    uint32_t _seed = 0;
    float _lensRadius = -1.0f;
    float _focusDistance = -1.0f;
};

} // namespace RayTracer
#endif // APP_HPP_
