/*
** EPITECH PROJECT, 2023
** raytracer
** File description:
** App.cpp
*/

#include "App.hpp"

using namespace RayTracer;

/**
 * Constructor of the App class.
 * Parse the arguments given by the user, using getopt.
 * Set the options for getopt to parse the arguments.
 * If the user set the path, interface, height or width, set the corresponding variables.
 *
 * @param ac The number of arguments.
 * @param av The input arguments.
 */
App::App(int ac, char **av)
    : _parser(std::make_unique<Parser>()), _renderer(std::make_unique<Renderer>()), _filepath(""),
      _output(""), _interface(Interface::NONE), _width(960), _height(540), _frames(10)
{
    int opt;

    static struct option options[] = {{"path", required_argument, NULL, 1},
                                      {"cli", 0, NULL, 2},
                                      {"sfml", 0, NULL, 3},
                                      {"height", required_argument, NULL, 4},
                                      {"width", required_argument, NULL, 5},
                                      {"frames", required_argument, NULL, 6},
                                      {"output", required_argument, NULL, 7},
                                      {"help", 0, NULL, 8},
                                      {NULL, 0, NULL, 0}};

    while ((opt = getopt_long(ac, av, "h", options, NULL)) != EOF)
    {
        switch (opt)
        {
        case 1:
            _filepath = optarg;
            break;
        case 2:
            _interface = Interface::CLI;
            break;
        case 3:
            _interface = Interface::GUI;
            break;
        case 4:
            _height = std::atoi(optarg);
            break;
        case 5:
            _width = std::atoi(optarg);
            break;
        case 6:
            _frames = std::atoi(optarg);
            break;
        case 7:
            _output = optarg;
            break;
        default:
            std::cout << "Usage: ./raytracer --path [path] --cli/--sfml --height [height] --width [width]" << std::endl;
            break;
        }
    }
}

App::~App(void)
{
}

/**
 * Run the application.
 * Start by parsing the scene and the camera.
 * Then render the scene.
 * If the interface is set to CLI, output the result in a PPM file.
 * If the interface is set to GUI, output the result using SFML.
 * If the interface or the path is not set, exit the program.
 */
void App::run(void)
{
    Factory factory;
    if (_filepath.empty() || _interface == Interface::NONE)
        exit(EXIT_FAILURE);

    std::shared_ptr<Scene> scene = _parser->init(_filepath);
    std::shared_ptr<Camera> camera = _parser->getParsedCamera();

    if (_interface == Interface::CLI)
    {
    }
    else if (_interface == Interface::GUI)
    {
        std::shared_ptr<IGUI> sfml = factory.create<IGUI>("SFML", _width, _height);
        std::unique_ptr<Renderer> renderer = std::make_unique<Renderer>(_frames, scene, camera, sfml, _output);

        renderer->run();
    }
}
