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
#include "Renderer.hpp"
#include <getopt.h>

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
    std::unique_ptr<Renderer> _renderer;
    std::string _filepath;
    std::string _output;
    Interface _interface;
    int _width;
    int _height;
    int _frames;
};

} // namespace RayTracer
#endif // APP_HPP_
