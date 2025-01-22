/*
** EPITECH PROJECT, 2024
** raytracer
** File description:
** SFML
*/

#include "SFML.hpp"
#include <fstream>
#include <iostream>
#include <memory>
#include <unordered_map>

using namespace RayTracer;

SFML::SFML(const std::size_t &width, const std::size_t &height)
    : _width(width), _height(height), _window(sf::VideoMode(width, height), "Raytracer")
{
    _pixels.resize(_height, std::vector<Color4D>(_width));
    textureUpdate();
}

SFML::SFML(const std::size_t &width, const std::size_t &height, const Pixels &pixels)
    : _width(width), _height(height), _pixels(pixels), _window(sf::VideoMode(width, height), "Raytracer")
{
    _pixels.resize(_height, std::vector<Color4D>(_width));
    textureUpdate();
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

    sf::Image image;
    image.create(_width, _height);
    _pixels.resize(_height, std::vector<Color4D>(_width));
    _window.close();
    _window.create(sf::VideoMode(_width, _height), "Raytracer");

    for (std::size_t y = 0; y < _height; ++y)
    {
        for (std::size_t x = 0; x < _width; ++x)
        {
            std::uint16_t color[3];
            file >> color[0] >> color[1] >> color[2];
            _pixels[y][x] = Color4D(color[0], color[1], color[2]);
            image.setPixel(x, y, sf::Color(color[0], color[1], color[2]));
        }
    }
    _texture.loadFromImage(image);
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
    _pixels = pixels;
    textureUpdate();
}

void SFML::setImage(const std::size_t &width, const std::size_t &height, std::vector<uint32_t> &pixels)
{
    sf::Uint8 *ptr = reinterpret_cast<sf::Uint8 *>(pixels.data());

    // std::cerr << (int)vec8[0] << " " << (int)vec8[1] << " " << (int)vec8[2] << " " << (int)vec8[3] << std::endl;

    // sf::Uint8* ptr = vec8.data();
    sf::Image img;
    img.create(width, height, ptr);
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
    if (x > _width || y > _height)
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

    sf::Sprite sprite(_texture);

    _window.clear();
    _window.draw(sprite);
    _window.display();
}

char SFML::getEvent()
{
    sf::Event event;

    if (!isOpen() || !_window.pollEvent(event))
        return 0;
    if (event.type == sf::Event::Closed ||
        (event.type == sf::Event::KeyPressed && event.key.code == sf::Keyboard::Escape))
    {
        _close = true;
        return 0;
    }
    if (event.type != sf::Event::KeyPressed)
        return 0;
    return sfKeyToAscii(event.key.code);
}

bool SFML::isOpen()
{
    return _window.isOpen() && !_close;
}

void SFML::textureUpdate()
{
    sf::Image image;
    image.create(_width, _height);

    for (std::size_t y = 0; !_pixels.empty() && y < _height; ++y)
    {
        for (std::size_t x = 0; x < _width; ++x)
        {
            Color4D color = _pixels[y][x];
            image.setPixel(x, y, sf::Color(color.getX(), color.getY(), color.getZ()));
        }
    }

    _texture.loadFromImage(image);
}

char SFML::sfKeyToAscii(sf::Keyboard::Key key)
{
    if (key == sf::Keyboard::Unknown)
        return '\0';
    if (key >= sf::Keyboard::A && key <= sf::Keyboard::Z)
        return 'a' + (key - sf::Keyboard::A);
    if (key >= sf::Keyboard::Num0 && key <= sf::Keyboard::Num9)
        return '0' + (key - sf::Keyboard::Num0);

    const std::unordered_map<sf::Keyboard::Key, char> keyMap = {
        {sf::Keyboard::Space, ' '}, {sf::Keyboard::Enter, '\n'}, {sf::Keyboard::Escape, 27}, {sf::Keyboard::Left, 'a'},
        {sf::Keyboard::Right, 'd'}, {sf::Keyboard::Up, 'w'},     {sf::Keyboard::Down, 's'}};

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
