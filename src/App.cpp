/*
** EPITECH PROJECT, 2023
** raytracer
** File description:
** App.cpp
*/

#include "App.hpp"
#include "plugins/renderers/IRenderer.hpp"
#include "plugins/renderers/RendererRegistry.hpp"
#include <cerrno>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <memory>
#include <string>

using namespace RayTracer;

namespace
{
const char *const USAGE =
    "Usage: ./raytracer --path <scene.cfg> --cli|--sfml [--renderer cpu|cuda] [--width W] [--height H] "
    "[--frames N] [--output out.ppm] [--list-renderers]";

void printUsage(std::ostream &stream)
{
    stream << USAGE << std::endl;
}

int parsePositiveInt(const char *value, const char *name)
{
    errno = 0;
    char *end = nullptr;
    const long parsed = std::strtol(value, &end, 10);

    if (errno != 0 || end == value || *end != '\0' || parsed <= 0 || parsed > std::numeric_limits<int>::max())
    {
        std::cerr << "Invalid " << name << " value: " << value << std::endl;
        printUsage(std::cerr);
        std::exit(EXIT_FAILURE);
    }

    return static_cast<int>(parsed);
}

void printRendererNames(std::ostream &stream)
{
    stream << "Available renderers:";
    for (const RendererEntry &entry : rendererEntries())
        stream << " " << entry.name;
    stream << std::endl;
}

void listRenderers(void)
{
    Factory factory;

    std::cout << "Available renderers:" << std::endl;
    for (const RendererEntry &entry : rendererEntries())
    {
        std::cout << "  " << entry.name << " - " << entry.description;

        try
        {
            std::shared_ptr<IRenderer> renderer = factory.create<IRenderer>(entry.factoryKey);
            std::string reason;

            if (renderer->isAvailable(reason))
                std::cout << " [available]";
            else
                std::cout << " [unavailable: " << reason << "]";
        }
        catch (const std::exception &error)
        {
            std::cout << " [unavailable: " << error.what() << "]";
        }

        std::cout << std::endl;
    }
}
} // namespace

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
    : _parser(std::make_unique<Parser>()), _filepath(""), _output(""), _rendererName("cpu"),
      _interface(Interface::NONE), _width(960), _height(540), _frames(10)
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
                                      {"renderer", required_argument, NULL, 9},
                                      {"list-renderers", 0, NULL, 10},
                                      {NULL, 0, NULL, 0}};

    // Let the application report unknown options consistently.
    opterr = 0;

    while ((opt = getopt_long(ac, av, "h", options, NULL)) != -1)
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
            _height = parsePositiveInt(optarg, "height");
            break;
        case 5:
            _width = parsePositiveInt(optarg, "width");
            break;
        case 6:
            _frames = parsePositiveInt(optarg, "frames");
            break;
        case 7:
            _output = optarg;
            break;
        case 8:
        case 'h':
            printUsage(std::cout);
            std::exit(EXIT_SUCCESS);
        case 9:
            _rendererName = optarg;
            break;
        case 10:
            listRenderers();
            std::exit(EXIT_SUCCESS);
        case '?':
        default:
            std::cerr << "Unknown or incomplete option." << std::endl;
            printUsage(std::cerr);
            std::exit(EXIT_FAILURE);
        }
    }
}

App::~App(void)
{
}

/**
 * Run the application.
 * Start by parsing the scene and the camera.
 * Then render the scene with the selected renderer plugin.
 * If the interface is set to CLI, output the result in a PPM file.
 * If the interface is set to GUI, output the result using SFML.
 * If the interface or the path is not set, exit the program.
 */
void App::run(void)
{
    if (_filepath.empty() || _interface == Interface::NONE)
    {
        printUsage(std::cerr);
        std::exit(EXIT_FAILURE);
    }
    if (_width <= 0 || _height <= 0)
    {
        std::cerr << "Width and height must be positive." << std::endl;
        std::exit(EXIT_FAILURE);
    }
    if (_frames <= 0)
    {
        std::cerr << "Frames must be positive." << std::endl;
        std::exit(EXIT_FAILURE);
    }

    try
    {
        Factory factory;
        std::shared_ptr<Scene> scene = _parser->init(_filepath);
        std::shared_ptr<Camera> camera = _parser->getParsedCamera();

        if (!scene || !camera)
        {
            std::cerr << "Failed to load the scene or camera from: " << _filepath << std::endl;
            std::exit(EXIT_FAILURE);
        }

        // The parser builds a camera with its default resolution, so apply the
        // requested render size before creating the renderer.
        camera->initialize(_width, _height, camera->getLookAt(), camera->getLookFrom(),
                           camera->getVerticalFovDegree());

        const RendererEntry *rendererEntry = findRenderer(_rendererName);
        if (!rendererEntry)
        {
            std::cerr << "Unknown renderer: " << _rendererName << std::endl;
            printRendererNames(std::cerr);
            std::exit(EXIT_FAILURE);
        }

        std::shared_ptr<IRenderer> renderer;
        try
        {
            renderer = factory.create<IRenderer>(rendererEntry->factoryKey);
        }
        catch (const std::exception &error)
        {
            std::cerr << "Renderer '" << rendererEntry->name << "' is unavailable: " << error.what() << std::endl;
            std::exit(EXIT_FAILURE);
        }

        std::string unavailableReason;

        if (!renderer->isAvailable(unavailableReason))
        {
            std::cerr << "Renderer '" << rendererEntry->name << "' is unavailable: " << unavailableReason << std::endl;
            std::exit(EXIT_FAILURE);
        }

        RenderContext context;

        context.scene = scene;
        context.camera = camera;
        context.options.width = _width;
        context.options.height = _height;
        context.options.frames = _frames;
        context.options.outputPath = _output;

        if (_interface == Interface::GUI)
            context.gui = factory.create<IGUI>("SFML", _width, _height);

        renderer->run(context);
    }
    catch (const std::exception &e)
    {
        std::cerr << "Error: " << e.what() << std::endl;
        std::exit(EXIT_FAILURE);
    }
}
