/*
** EPITECH PROJECT, 2024
** raytracer
** File description:
** OpenGL GUI
*/

#include "OpenGL.hpp"
#include <GL/glew.h>
#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>
#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdint>
#include <deque>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

namespace RayTracer
{
namespace
{
const char *const VERTEX_SHADER = R"(
#version 330 core
out vec2 texCoord;

void main()
{
    const vec2 positions[3] = vec2[3](
        vec2(-1.0, -1.0),
        vec2( 3.0, -1.0),
        vec2(-1.0,  3.0)
    );

    vec2 position = positions[gl_VertexID];
    gl_Position = vec4(position, 0.0, 1.0);
    texCoord = position * 0.5 + 0.5;
}
)";

const char *const FRAGMENT_SHADER = R"(
#version 330 core
in vec2 texCoord;
out vec4 color;
uniform sampler2D image;

void main()
{
    color = texture(image, vec2(texCoord.x, 1.0 - texCoord.y));
}
)";

void glfwErrorCallback(int error, const char *description)
{
    std::cerr << "GLFW error " << error << ": " << (description != nullptr ? description : "") << std::endl;
}

std::string shaderLog(GLuint shader)
{
    GLint length = 0;

    glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &length);
    if (length <= 1)
        return {};

    std::string log(static_cast<std::size_t>(length), '\0');

    glGetShaderInfoLog(shader, length, nullptr, log.data());
    return log;
}

std::string programLog(GLuint program)
{
    GLint length = 0;

    glGetProgramiv(program, GL_INFO_LOG_LENGTH, &length);
    if (length <= 1)
        return {};

    std::string log(static_cast<std::size_t>(length), '\0');

    glGetProgramInfoLog(program, length, nullptr, log.data());
    return log;
}

GLuint compileShader(GLenum type, const char *source)
{
    const GLuint shader = glCreateShader(type);

    if (shader == 0)
        throw std::runtime_error("OpenGL could not create a shader");

    glShaderSource(shader, 1, &source, nullptr);
    glCompileShader(shader);

    GLint compiled = GL_FALSE;

    glGetShaderiv(shader, GL_COMPILE_STATUS, &compiled);
    if (compiled != GL_TRUE)
    {
        const std::string log = shaderLog(shader);

        glDeleteShader(shader);
        throw std::runtime_error("OpenGL shader compilation failed: " + log);
    }

    return shader;
}

GLuint linkProgram(GLuint vertexShader, GLuint fragmentShader)
{
    const GLuint program = glCreateProgram();

    if (program == 0)
    {
        glDeleteShader(vertexShader);
        glDeleteShader(fragmentShader);
        throw std::runtime_error("OpenGL could not create a shader program");
    }

    glAttachShader(program, vertexShader);
    glAttachShader(program, fragmentShader);
    glLinkProgram(program);

    glDeleteShader(vertexShader);
    glDeleteShader(fragmentShader);

    GLint linked = GL_FALSE;

    glGetProgramiv(program, GL_LINK_STATUS, &linked);
    if (linked != GL_TRUE)
    {
        const std::string log = programLog(program);

        glDeleteProgram(program);
        throw std::runtime_error("OpenGL program link failed: " + log);
    }

    return program;
}

std::uint8_t colorToByte(float value)
{
    if (!std::isfinite(value))
        return 0;

    return static_cast<std::uint8_t>(std::lround(std::clamp(value, 0.0f, 1.0f) * 255.0f));
}

std::uint32_t toRGBA(const Color4D &color)
{
    const std::uint8_t r = colorToByte(color.getX());
    const std::uint8_t g = colorToByte(color.getY());
    const std::uint8_t b = colorToByte(color.getZ());
    const std::uint8_t a = colorToByte(color.getW());

    return (static_cast<std::uint32_t>(a) << 24u) | (static_cast<std::uint32_t>(b) << 16u) |
           (static_cast<std::uint32_t>(g) << 8u) | static_cast<std::uint32_t>(r);
}

char keyToChar(int key)
{
    if (key >= GLFW_KEY_A && key <= GLFW_KEY_Z)
        return static_cast<char>('a' + (key - GLFW_KEY_A));
    if (key >= GLFW_KEY_0 && key <= GLFW_KEY_9)
        return static_cast<char>('0' + (key - GLFW_KEY_0));

    switch (key)
    {
    case GLFW_KEY_SPACE:
        return ' ';
    case GLFW_KEY_ENTER:
    case GLFW_KEY_KP_ENTER:
        return '\n';
    case GLFW_KEY_LEFT:
        return 'j';
    case GLFW_KEY_RIGHT:
        return 'l';
    case GLFW_KEY_UP:
        return 'i';
    case GLFW_KEY_DOWN:
        return 'k';
    case GLFW_KEY_EQUAL:
    case GLFW_KEY_KP_ADD:
        return '+';
    case GLFW_KEY_MINUS:
    case GLFW_KEY_KP_SUBTRACT:
        return '-';
    case GLFW_KEY_LEFT_BRACKET:
        return '[';
    case GLFW_KEY_RIGHT_BRACKET:
        return ']';
    case GLFW_KEY_COMMA:
        return ',';
    case GLFW_KEY_PERIOD:
        return '.';
    case GLFW_KEY_SEMICOLON:
        return ';';
    case GLFW_KEY_APOSTROPHE:
        return '\'';
    default:
        return '\0';
    }
}

struct PPMImage
{
    std::size_t width = 0;
    std::size_t height = 0;
    std::vector<std::uint32_t> pixels;
};

PPMImage readPPM(const std::string &path)
{
    std::ifstream file(path, std::ios::binary);

    if (!file)
        throw std::runtime_error("Cannot open file: " + path);

    std::string magic;
    int width = 0;
    int height = 0;
    int maxValue = 0;

    file >> magic >> width >> height >> maxValue;

    if (magic != "P6")
        throw std::runtime_error("Unsupported PPM format (expected binary P6): " + path);
    if (width <= 0 || height <= 0)
        throw std::runtime_error("Invalid dimensions in PPM file: " + path);
    if (maxValue != 255)
        throw std::runtime_error("Invalid max color value in PPM file: " + path);

    char separator = '\0';

    file.get(separator);
    if (!std::isspace(static_cast<unsigned char>(separator)))
        throw std::runtime_error("Malformed PPM header: " + path);
    if (separator == '\r')
    {
        char lineFeed = '\0';

        file.get(lineFeed);
        if (lineFeed != '\n')
            throw std::runtime_error("Malformed PPM line ending: " + path);
    }

    constexpr std::size_t MAX_PIXELS = 16000000;
    const std::size_t pixelCount = static_cast<std::size_t>(width) * static_cast<std::size_t>(height);

    if (pixelCount > MAX_PIXELS)
        throw std::runtime_error("PPM image is too large: " + path);

    std::vector<std::uint8_t> rgb(pixelCount * 3u);

    file.read(reinterpret_cast<char *>(rgb.data()), static_cast<std::streamsize>(rgb.size()));
    if (file.gcount() != static_cast<std::streamsize>(rgb.size()))
        throw std::runtime_error("Truncated PPM pixel data: " + path);

    PPMImage image;
    image.width = static_cast<std::size_t>(width);
    image.height = static_cast<std::size_t>(height);
    image.pixels.resize(pixelCount);

    for (std::size_t i = 0; i < pixelCount; ++i)
    {
        const std::uint8_t r = rgb[i * 3u];
        const std::uint8_t g = rgb[i * 3u + 1u];
        const std::uint8_t b = rgb[i * 3u + 2u];

        image.pixels[i] = (0xffu << 24u) | (static_cast<std::uint32_t>(b) << 16u) |
                          (static_cast<std::uint32_t>(g) << 8u) | static_cast<std::uint32_t>(r);
    }

    return image;
}
} // namespace

struct OpenGLGUI::Impl
{
    GLFWwindow *window = nullptr;
    bool glfwInitialized = false;
    GLuint program = 0;
    GLuint vao = 0;
    GLuint texture = 0;
    GLint imageUniform = -1;
    std::size_t imageWidth = 0;
    std::size_t imageHeight = 0;
    std::size_t textureWidth = 0;
    std::size_t textureHeight = 0;
    std::deque<char> events;

    explicit Impl(const std::size_t &width, const std::size_t &height)
    {
        glfwSetErrorCallback(glfwErrorCallback);

#if defined(__linux__) && defined(GLFW_PLATFORM) && defined(GLFW_PLATFORM_X11)
        glfwInitHint(GLFW_PLATFORM, GLFW_PLATFORM_X11);
#endif

        if (!glfwInit())
            throw std::runtime_error("Failed to initialize GLFW");
        glfwInitialized = true;

        glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
        glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
        glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
        glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GLFW_TRUE);
        glfwWindowHint(GLFW_CONTEXT_CREATION_API, GLFW_NATIVE_CONTEXT_API);

        window = glfwCreateWindow(static_cast<int>(std::max<std::size_t>(width, 1)),
                                  static_cast<int>(std::max<std::size_t>(height, 1)), "Raytracer", nullptr, nullptr);
        if (window == nullptr)
        {
            cleanup();
            throw std::runtime_error("Failed to create an OpenGL window");
        }

        glfwMakeContextCurrent(window);
        glfwSwapInterval(1);

        glewExperimental = GL_TRUE;
        const GLenum glewStatus = glewInit();

        if (glewStatus != GLEW_OK)
        {
            const std::string message = reinterpret_cast<const char *>(glewGetErrorString(glewStatus));

            cleanup();
            throw std::runtime_error("Failed to initialize GLEW: " + message + " (status " +
                                     std::to_string(glewStatus) + ")");
        }

        // GLEW can leave a benign GL_INVALID_ENUM behind on core profiles.
        while (glGetError() != GL_NO_ERROR)
        {
        }

        glfwSetWindowUserPointer(window, this);
        glfwSetKeyCallback(window, keyCallback);
        glfwSetFramebufferSizeCallback(window, framebufferSizeCallback);

        try
        {
            createResources();
        }
        catch (...)
        {
            cleanup();
            throw;
        }
    }

    ~Impl(void)
    {
        cleanup();
    }

    void cleanup(void)
    {
        if (window != nullptr)
            glfwMakeContextCurrent(window);

        if (program != 0)
        {
            glDeleteProgram(program);
            program = 0;
        }
        if (vao != 0)
        {
            glDeleteVertexArrays(1, &vao);
            vao = 0;
        }
        if (texture != 0)
        {
            glDeleteTextures(1, &texture);
            texture = 0;
        }

        if (window != nullptr)
        {
            glfwDestroyWindow(window);
            window = nullptr;
        }

        if (glfwInitialized)
        {
            glfwTerminate();
            glfwInitialized = false;
        }
    }

    void createResources(void)
    {
        GLuint vertexShader = 0;
        GLuint fragmentShader = 0;

        try
        {
            vertexShader = compileShader(GL_VERTEX_SHADER, VERTEX_SHADER);
            fragmentShader = compileShader(GL_FRAGMENT_SHADER, FRAGMENT_SHADER);
        }
        catch (...)
        {
            if (vertexShader != 0)
                glDeleteShader(vertexShader);
            if (fragmentShader != 0)
                glDeleteShader(fragmentShader);
            throw;
        }

        program = linkProgram(vertexShader, fragmentShader);
        glUseProgram(program);

        imageUniform = glGetUniformLocation(program, "image");
        if (imageUniform < 0)
            throw std::runtime_error("OpenGL shader is missing the image sampler");

        glGenVertexArrays(1, &vao);
        glBindVertexArray(vao);

        glGenTextures(1, &texture);
        glBindTexture(GL_TEXTURE_2D, texture);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

        glDisable(GL_DEPTH_TEST);
        glDisable(GL_CULL_FACE);
        glDisable(GL_BLEND);
        glUseProgram(0);
        glBindVertexArray(0);
    }

    void setImage(const std::size_t &width, const std::size_t &height, const std::vector<uint32_t> &pixels)
    {
        if (width == 0 || height == 0 || pixels.size() < width * height)
            return;

        glBindTexture(GL_TEXTURE_2D, texture);
        glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

        if (textureWidth != width || textureHeight != height)
        {
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, static_cast<GLsizei>(width), static_cast<GLsizei>(height), 0,
                         GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());
            textureWidth = width;
            textureHeight = height;
        }
        else
        {
            glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, static_cast<GLsizei>(width), static_cast<GLsizei>(height),
                            GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());
        }

        imageWidth = width;
        imageHeight = height;
    }

    void setPixels(const std::vector<std::vector<Color4D>> &pixels)
    {
        if (pixels.empty())
            return;

        std::size_t width = 0;

        for (const std::vector<Color4D> &row : pixels)
            width = std::max(width, row.size());

        if (width == 0)
            return;

        const std::size_t height = pixels.size();
        std::vector<uint32_t> rgba(width * height);

        for (std::size_t y = 0; y < height; ++y)
        {
            for (std::size_t x = 0; x < width; ++x)
            {
                const Color4D color = x < pixels[y].size() ? pixels[y][x] : Color4D();

                rgba[x + y * width] = toRGBA(color);
            }
        }

        setImage(width, height, rgba);
    }

    void setPixel(const std::size_t &x, const std::size_t &y, const Color4D &color)
    {
        if (textureWidth == 0 || x >= textureWidth || y >= textureHeight)
            return;

        const std::uint32_t pixel = toRGBA(color);

        glBindTexture(GL_TEXTURE_2D, texture);
        glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
        glTexSubImage2D(GL_TEXTURE_2D, 0, static_cast<GLint>(x), static_cast<GLint>(y), 1, 1, GL_RGBA,
                        GL_UNSIGNED_BYTE, &pixel);
    }

    void display(void)
    {
        if (window == nullptr)
            return;

        glfwPollEvents();
        if (glfwWindowShouldClose(window))
            return;

        int framebufferWidth = 0;
        int framebufferHeight = 0;

        glfwGetFramebufferSize(window, &framebufferWidth, &framebufferHeight);
        if (framebufferWidth <= 0 || framebufferHeight <= 0)
            return;

        glViewport(0, 0, framebufferWidth, framebufferHeight);
        glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);

        if (imageWidth > 0 && imageHeight > 0)
        {
            int viewportWidth = framebufferWidth;
            int viewportHeight = framebufferHeight;
            const float imageAspect = static_cast<float>(imageWidth) / static_cast<float>(imageHeight);
            const float windowAspect = static_cast<float>(framebufferWidth) / static_cast<float>(framebufferHeight);

            if (imageAspect > windowAspect)
                viewportHeight = std::max(1, static_cast<int>(static_cast<float>(framebufferWidth) / imageAspect));
            else
                viewportWidth = std::max(1, static_cast<int>(static_cast<float>(framebufferHeight) * imageAspect));

            glViewport((framebufferWidth - viewportWidth) / 2, (framebufferHeight - viewportHeight) / 2,
                       viewportWidth, viewportHeight);

            glUseProgram(program);
            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, texture);
            glUniform1i(imageUniform, 0);
            glBindVertexArray(vao);
            glDrawArrays(GL_TRIANGLES, 0, 3);
            glBindVertexArray(0);
            glUseProgram(0);
        }

        glfwSwapBuffers(window);
    }

    bool isOpen(void) const
    {
        return window != nullptr && !glfwWindowShouldClose(window);
    }

    char getEvent(void)
    {
        if (window == nullptr)
            return 0;

        if (events.empty())
            glfwPollEvents();

        if (events.empty())
            return 0;

        const char event = events.front();

        events.pop_front();
        return event;
    }

    static void keyCallback(GLFWwindow *handle, int key, int, int action, int)
    {
        Impl *self = static_cast<Impl *>(glfwGetWindowUserPointer(handle));

        if (self == nullptr || action == GLFW_RELEASE)
            return;

        if (key == GLFW_KEY_ESCAPE)
        {
            glfwSetWindowShouldClose(handle, GLFW_TRUE);
            return;
        }

        const char value = keyToChar(key);

        if (value != '\0')
            self->events.push_back(value);
    }

    static void framebufferSizeCallback(GLFWwindow *, int, int)
    {
        // The display loop queries the framebuffer size every frame, so no
        // cached state is required here.
    }
};

OpenGLGUI::OpenGLGUI(const std::size_t &width, const std::size_t &height)
    : _impl(std::make_unique<Impl>(width, height))
{
}

OpenGLGUI::~OpenGLGUI(void) = default;

void OpenGLGUI::loadPPM(const std::string &path)
{
    const PPMImage image = readPPM(path);

    glfwSetWindowSize(_impl->window, static_cast<int>(image.width), static_cast<int>(image.height));
    _impl->setImage(image.width, image.height, image.pixels);
}

void OpenGLGUI::setImage(const std::size_t &width, const std::size_t &height,
                         const std::vector<uint32_t> &pixels)
{
    _impl->setImage(width, height, pixels);
}

void OpenGLGUI::setSize(const std::size_t &width, const std::size_t &height)
{
    glfwSetWindowSize(_impl->window, static_cast<int>(std::max<std::size_t>(width, 1)),
                      static_cast<int>(std::max<std::size_t>(height, 1)));
}

void OpenGLGUI::setPixels(const std::vector<std::vector<Color4D>> &pixels)
{
    _impl->setPixels(pixels);
}

void OpenGLGUI::setPixel(const std::size_t &x, const std::size_t &y, const Color4D &color)
{
    _impl->setPixel(x, y, color);
}

void OpenGLGUI::display(void)
{
    _impl->display();
}

void OpenGLGUI::setTitle(const std::string &title)
{
    if (_impl->window != nullptr)
        glfwSetWindowTitle(_impl->window, title.c_str());
}

bool OpenGLGUI::isOpen(void)
{
    return _impl->isOpen();
}

char OpenGLGUI::getEvent(void)
{
    return _impl->getEvent();
}

void OpenGLGUI::loop(void)
{
    while (isOpen())
    {
        getEvent();
        display();
    }
}

extern "C" std::shared_ptr<IGUI> create(int width, int height)
{
    return std::make_shared<OpenGLGUI>(static_cast<std::size_t>(std::max(width, 1)),
                                       static_cast<std::size_t>(std::max(height, 1)));
}
} // namespace RayTracer
