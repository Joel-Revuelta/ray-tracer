/*
** EPITECH PROJECT, 2024
** raytracer
** File description:
** SFML
*/

#include "SFML.hpp"
#include <algorithm>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <memory>
#include <optional>
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

    std::string header;
    int maxColorValue;
    file >> header >> _width >> _height >> maxColorValue;

    if (maxColorValue != 255)
        throw std::runtime_error("Invalid max color value in PPM file: " + path);

    sf::Image image({static_cast<unsigned int>(_width), static_cast<unsigned int>(_height)});
    _pixels.resize(_height, std::vector<Color4D>(_width));
    _window.close();
    _window.create(sf::VideoMode({static_cast<unsigned int>(_width), static_cast<unsigned int>(_height)}), "Raytracer");

    for (std::size_t y = 0; y < _height; ++y)
    {
        for (std::size_t x = 0; x < _width; ++x)
        {
            std::uint16_t color[3];
            file >> color[0] >> color[1] >> color[2];
            _pixels[y][x] = Color4D(color[0], color[1], color[2]);
            image.setPixel({static_cast<unsigned int>(x), static_cast<unsigned int>(y)},
                           sf::Color(color[0], color[1], color[2]));
        }
    }
    if (!_texture.loadFromImage(image))
        throw std::runtime_error("Failed to load texture from image: " + path);
    file.close();
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

void SFML::setImage(const std::size_t &width, const std::size_t &height, std::vector<uint32_t> &pixels)
{
    if (width == 0 || height == 0 || pixels.size() < width * height)
    {
        std::cerr << "Invalid image data" << std::endl;
        return;
    }

    std::uint8_t *ptr = reinterpret_cast<std::uint8_t *>(pixels.data());

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
        {sf::Keyboard::Key::Space, ' '}, {sf::Keyboard::Key::Enter, '\n'}, {sf::Keyboard::Key::Escape, 27},
        {sf::Keyboard::Key::Left, 'a'},  {sf::Keyboard::Key::Right, 'd'}, {sf::Keyboard::Key::Up, 'w'},
        {sf::Keyboard::Key::Down, 's'}};

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
