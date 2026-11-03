/*
** EPITECH PROJECT, 2023
** raytracer
** File description:
** App.cpp
*/

#include "App.hpp"
#include "plugins/renderers/IRenderer.hpp"
#include "plugins/renderers/RendererRegistry.hpp"
#include <algorithm>
#include <cctype>
#include <cerrno>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <memory>
#include <string>

using namespace RayTracer;

namespace
{
const char *const USAGE =
    "Usage: ./raytracer --path <scene.cfg> --cli|--sfml|--gui sfml|opengl "
    "[--renderer cpu|cuda] [--width W] [--height H] [--frames N] [--threads N] [--seed N] "
    "[--device N] [--lens-radius R] [--focus-distance D] [--exposure E] "
    "[--tonemap none|reinhard|aces] [--output out.ppm] [--list-renderers] [--list-guis]";

std::string normalizeGuiName(std::string name)
{
    std::transform(name.begin(), name.end(), name.begin(),
                   [](unsigned char character) { return static_cast<char>(std::tolower(character)); });

    if (name == "sfml")
        return "SFML";
    if (name == "opengl")
        return "OpenGL";
    return {};
}

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

int parseNonNegativeInt(const char *value, const char *name)
{
    errno = 0;
    char *end = nullptr;
    const long parsed = std::strtol(value, &end, 10);

    if (errno != 0 || end == value || *end != '\0' || parsed < 0 || parsed > std::numeric_limits<int>::max())
    {
        std::cerr << "Invalid " << name << " value: " << value << std::endl;
        printUsage(std::cerr);
        std::exit(EXIT_FAILURE);
    }

    return static_cast<int>(parsed);
}

void listGuis(void)
{
    std::cout << "Available GUI backends:" << std::endl;
    std::cout << "  sfml - SFML 3 window" << std::endl;
    std::cout << "  opengl - GLFW/OpenGL 3.3 window" << std::endl;
}

float parseNonNegativeFloat(const char *value, const char *name)
{
    errno = 0;
    char *end = nullptr;
    const float parsed = std::strtof(value, &end);

    if (errno != 0 || end == value || *end != '\0' || !std::isfinite(parsed) || parsed < 0.0f)
    {
        std::cerr << "Invalid " << name << " value: " << value << std::endl;
        printUsage(std::cerr);
        std::exit(EXIT_FAILURE);
    }

    return parsed;
}

float parsePositiveFloat(const char *value, const char *name)
{
    const float parsed = parseNonNegativeFloat(value, name);

    if (parsed <= 0.0f)
    {
        std::cerr << "Invalid " << name << " value: " << value << std::endl;
        printUsage(std::cerr);
        std::exit(EXIT_FAILURE);
    }

    return parsed;
}

bool parseToneMapping(const char *value, ToneMapping &mapping)
{
    std::string name(value);

    std::transform(name.begin(), name.end(), name.begin(),
                   [](unsigned char character) { return static_cast<char>(std::tolower(character)); });

    if (name == "none")
        mapping = ToneMapping::NONE;
    else if (name == "reinhard")
        mapping = ToneMapping::REINHARD;
    else if (name == "aces")
        mapping = ToneMapping::ACES;
    else
        return false;
    return true;
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
      _guiName("SFML"), _postProcess(), _interface(Interface::NONE), _width(960), _height(540), _frames(10)
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
                                      {"gui", required_argument, NULL, 11},
                                      {"exposure", required_argument, NULL, 12},
                                      {"tonemap", required_argument, NULL, 13},
                                      {"threads", required_argument, NULL, 14},
                                      {"seed", required_argument, NULL, 15},
                                      {"list-guis", 0, NULL, 16},
                                      {"device", required_argument, NULL, 17},
                                      {"lens-radius", required_argument, NULL, 18},
                                      {"focus-distance", required_argument, NULL, 19},
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
            _guiName = "SFML";
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
        case 11:
            _interface = Interface::GUI;
            _guiName = optarg;
            break;
        case 12:
            _postProcess.exposure = parseNonNegativeFloat(optarg, "exposure");
            break;
        case 13:
            if (!parseToneMapping(optarg, _postProcess.toneMapping))
            {
                std::cerr << "Unknown tonemapping mode: " << optarg << std::endl;
                std::cerr << "Available modes: none, reinhard, aces" << std::endl;
                std::exit(EXIT_FAILURE);
            }
            break;
        case 14:
            _threads = parseNonNegativeInt(optarg, "threads");
            break;
        case 15:
            _seed = static_cast<uint32_t>(parseNonNegativeInt(optarg, "seed"));
            break;
        case 16:
            listGuis();
            std::exit(EXIT_SUCCESS);
        case 17:
            _device = parseNonNegativeInt(optarg, "device");
            break;
        case 18:
            _lensRadius = parseNonNegativeFloat(optarg, "lens radius");
            break;
        case 19:
            _focusDistance = parsePositiveFloat(optarg, "focus distance");
            break;
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
 * If the interface is set to GUI, output the result using the selected backend.
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

        if (_lensRadius >= 0.0f)
            camera->setLensRadius(_lensRadius);
        if (_focusDistance > 0.0f)
            camera->setFocusDistance(_focusDistance);

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
        context.options.threads = _threads;
        context.options.device = _device;
        context.options.seed = _seed;
        context.options.outputPath = _output;
        context.options.postProcess = _postProcess;

        if (_interface == Interface::GUI)
        {
            const std::string guiName = normalizeGuiName(_guiName);

            if (guiName.empty())
            {
                std::cerr << "Unknown GUI backend: " << _guiName << std::endl;
                std::cerr << "Available GUI backends: sfml, opengl" << std::endl;
                std::exit(EXIT_FAILURE);
            }

            try
            {
                context.gui = factory.create<IGUI>(guiName, _width, _height);
            }
            catch (const std::exception &error)
            {
                std::cerr << "GUI '" << _guiName << "' is unavailable: " << error.what() << std::endl;
                std::exit(EXIT_FAILURE);
            }
        }

        renderer->run(context);
    }
    catch (const std::exception &e)
    {
        std::cerr << "Error: " << e.what() << std::endl;
        std::exit(EXIT_FAILURE);
    }
}
