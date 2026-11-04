/*
** EPITECH PROJECT, 2024
** raytracer
** File description:
** SFML
*/

#include "SFML.hpp"
#include <algorithm>
#include <cctype>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <memory>
#include <optional>
#include <stdexcept>
#include <unordered_map>

using namespace RayTracer;

SFML::SFML(const std::size_t &width, const std::size_t &height)
    : _width(width), _height(height),
      _window(sf::VideoMode({static_cast<unsigned int>(width), static_cast<unsigned int>(height)}), "Raytracer")
{
    _pixels.resize(_height, std::vector<Color4D>(_width));
    textureUpdate();
}

SFML::SFML(const std::size_t &width, const std::size_t &height, const Pixels &pixels)
    : _width(width), _height(height),
      _window(sf::VideoMode({static_cast<unsigned int>(width), static_cast<unsigned int>(height)}), "Raytracer")
{
    setPixels(pixels);
}

SFML::SFML(const std::string &path) : _width(0), _height(0), _window()
{
    loadPPM(path);
}

SFML::~SFML()
{
    _window.close();
}

void SFML::loadPPM(const std::string &path)
{
    std::ifstream file(path, std::ios::binary);
    if (!file)
        throw std::runtime_error("Cannot open file: " + path);

    std::string magic;
    int width = 0;
    int height = 0;
    int maxColorValue = 0;

    file >> magic >> width >> height >> maxColorValue;

    if (magic != "P6")
        throw std::runtime_error("Unsupported PPM format (expected binary P6): " + path);
    if (width <= 0 || height <= 0)
        throw std::runtime_error("Invalid dimensions in PPM file: " + path);
    if (maxColorValue != 255)
        throw std::runtime_error("Invalid max color value in PPM file: " + path);

    // The header is followed by exactly one whitespace character. Accept CRLF
    // as well, but do not use std::ws here: the pixel data itself may start
    // with a whitespace-valued byte.
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
    const std::size_t pixelCount =
        static_cast<std::size_t>(width) * static_cast<std::size_t>(height);

    if (pixelCount > MAX_PIXELS)
        throw std::runtime_error("PPM image is too large: " + path);

    std::vector<std::uint8_t> rgb(pixelCount * 3u);

    file.read(reinterpret_cast<char *>(rgb.data()), static_cast<std::streamsize>(rgb.size()));
    if (file.gcount() != static_cast<std::streamsize>(rgb.size()))
        throw std::runtime_error("Truncated PPM pixel data: " + path);

    _width = static_cast<std::size_t>(width);
    _height = static_cast<std::size_t>(height);

    sf::Image image({static_cast<unsigned int>(_width), static_cast<unsigned int>(_height)});
    _pixels.assign(_height, std::vector<Color4D>(_width));
    _window.close();
    _window.create(sf::VideoMode({static_cast<unsigned int>(_width), static_cast<unsigned int>(_height)}), "Raytracer");

    for (std::size_t y = 0; y < _height; ++y)
    {
        for (std::size_t x = 0; x < _width; ++x)
        {
            const std::size_t offset = (x + y * _width) * 3u;
            const std::uint8_t r = rgb[offset];
            const std::uint8_t g = rgb[offset + 1u];
            const std::uint8_t b = rgb[offset + 2u];

            _pixels[y][x] = Color4D(r, g, b);
            image.setPixel({static_cast<unsigned int>(x), static_cast<unsigned int>(y)}, sf::Color(r, g, b));
        }
    }

    if (!_texture.loadFromImage(image))
        throw std::runtime_error("Failed to load texture from image: " + path);
}

void SFML::setSize(const std::size_t &width, const std::size_t &height)
{
    _width = width;
    _height = height;
    _window.setSize(sf::Vector2u(_width, _height));
    _pixels.clear();
    _pixels.resize(_height, std::vector<Color4D>(_width));
    textureUpdate();
}

void SFML::setPixels(const Pixels &pixels)
{
    _pixels.resize(_height);

    for (std::size_t y = 0; y < _height; ++y)
    {
        _pixels[y].resize(_width);

        if (y >= pixels.size())
            continue;

        const std::size_t copyWidth = std::min(_width, pixels[y].size());
        std::copy_n(pixels[y].begin(), copyWidth, _pixels[y].begin());
    }

    textureUpdate();
}

void SFML::setImage(const std::size_t &width, const std::size_t &height, const std::vector<uint32_t> &pixels)
{
    if (width == 0 || height == 0 || pixels.size() < width * height)
    {
        std::cerr << "Invalid image data" << std::endl;
        return;
    }

    const std::uint8_t *ptr = reinterpret_cast<const std::uint8_t *>(pixels.data());

    // std::cerr << (int)vec8[0] << " " << (int)vec8[1] << " " << (int)vec8[2] << " " << (int)vec8[3] << std::endl;

    // std::uint8_t* ptr = vec8.data();
    sf::Image img({static_cast<unsigned int>(width), static_cast<unsigned int>(height)}, ptr);
    if (!_texture.loadFromImage(img))
    {
        std::cerr << "Failed to load texture from image" << std::endl;
        return;
    }
    _width = width;
    _height = height;
}

void SFML::setPixel(const std::size_t &x, const std::size_t &y, const Color4D &color)
{
    if (y >= _pixels.size() || x >= _pixels[y].size())
        return;

    _pixels[y][x] = color;
    textureUpdate();
}

void SFML::display()
{
    if (_close)
        _window.close();
    if (!isOpen())
        return;

    const sf::Vector2u textureSize = _texture.getSize();
    if (textureSize.x == 0 || textureSize.y == 0)
        return;

    // Keep the whole image visible and stretch it to the current window size.
    const sf::View view(sf::FloatRect(sf::Vector2f(0.0f, 0.0f),
                                      sf::Vector2f(static_cast<float>(textureSize.x),
                                                   static_cast<float>(textureSize.y))));
    _window.setView(view);

    sf::Sprite sprite(_texture);

    _window.clear();
    _window.draw(sprite);
    _window.display();
}

char SFML::getEvent()
{
    if (!isOpen())
        return 0;

    const std::optional event = _window.pollEvent();
    if (!event)
        return 0;
    if (event->is<sf::Event::Closed>())
    {
        _close = true;
        return 0;
    }

    const auto *keyPressed = event->getIf<sf::Event::KeyPressed>();
    if (!keyPressed)
        return 0;
    if (keyPressed->code == sf::Keyboard::Key::Escape)
    {
        _close = true;
        return 0;
    }
    return sfKeyToAscii(keyPressed->code);
}

void SFML::setTitle(const std::string &title)
{
    _window.setTitle(title);
}

bool SFML::isOpen()
{
    return _window.isOpen() && !_close;
}

void SFML::textureUpdate()
{
    sf::Image image({static_cast<unsigned int>(_width), static_cast<unsigned int>(_height)});

    for (std::size_t y = 0; y < _height && y < _pixels.size(); ++y)
    {
        for (std::size_t x = 0; x < _width && x < _pixels[y].size(); ++x)
        {
            Color4D color = _pixels[y][x];
            image.setPixel({static_cast<unsigned int>(x), static_cast<unsigned int>(y)},
                           sf::Color(color.getX(), color.getY(), color.getZ()));
        }
    }

    if (!_texture.loadFromImage(image))
        std::cerr << "Failed to load texture from image" << std::endl;
}

char SFML::sfKeyToAscii(sf::Keyboard::Key key)
{
    if (key == sf::Keyboard::Key::Unknown)
        return '\0';
    if (key >= sf::Keyboard::Key::A && key <= sf::Keyboard::Key::Z)
        return static_cast<char>('a' + (static_cast<int>(key) - static_cast<int>(sf::Keyboard::Key::A)));
    if (key >= sf::Keyboard::Key::Num0 && key <= sf::Keyboard::Key::Num9)
        return static_cast<char>('0' + (static_cast<int>(key) - static_cast<int>(sf::Keyboard::Key::Num0)));

    const std::unordered_map<sf::Keyboard::Key, char> keyMap = {
        {sf::Keyboard::Key::Space, ' '},  {sf::Keyboard::Key::Enter, '\n'},
        {sf::Keyboard::Key::Escape, 27},  {sf::Keyboard::Key::Left, 'j'},
        {sf::Keyboard::Key::Right, 'l'},  {sf::Keyboard::Key::Up, 'i'},
        {sf::Keyboard::Key::Down, 'k'},   {sf::Keyboard::Key::Equal, '+'},
        {sf::Keyboard::Key::Add, '+'},    {sf::Keyboard::Key::Hyphen, '-'},
        {sf::Keyboard::Key::Subtract, '-'}, {sf::Keyboard::Key::LBracket, '['},
        {sf::Keyboard::Key::RBracket, ']'}, {sf::Keyboard::Key::Comma, ','},
        {sf::Keyboard::Key::Period, '.'}, {sf::Keyboard::Key::Semicolon, ';'},
        {sf::Keyboard::Key::Apostrophe, '\''}};

    auto it = keyMap.find(key);
    if (it != keyMap.end())
        return it->second;

    return '\0';
}

void SFML::loop()
{
    while (isOpen())
    {
        getEvent();
        display();
    }
}

extern "C" std::shared_ptr<IGUI> create(int width, int height)
{
    return std::make_shared<SFML>(width, height);
}
