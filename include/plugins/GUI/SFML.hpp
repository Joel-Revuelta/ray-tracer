/*
** EPITECH PROJECT, 2024
** raytracer
** File description:
** SFML
*/

#ifndef SFML_HPP_
#define SFML_HPP_

#include "GUI.hpp"
#include <SFML/Graphics.hpp>
#include <SFML/Window.hpp>

namespace RayTracer
{

class SFML : public IGUI
{
  public:
    using Pixels = std::vector<std::vector<Color4D>>;

    SFML(const std::size_t &width, const std::size_t &height);
    SFML(const std::size_t &width, const std::size_t &height, const Pixels &pixels);
    SFML(const std::string &path);
    ~SFML();

    void loadPPM(const std::string &path) override;
    void setImage(const std::size_t &width, const std::size_t &height, std::vector<uint32_t> &pixels) override;
    void setSize(const std::size_t &width, const std::size_t &height) override;
    void setPixels(const Pixels &pixels) override;
    void setPixel(const std::size_t &x, const std::size_t &y, const Color4D &color) override;
    void display() override;
    bool isOpen() override;
    char getEvent() override;
    void loop() override;

  private:
    std::size_t _width;
    std::size_t _height;
    Pixels _pixels;
    sf::RenderWindow _window;
    sf::Texture _texture;
    bool _close = false;

    void textureUpdate();
    char sfKeyToAscii(sf::Keyboard::Key key);
};

} // namespace RayTracer

#endif /* !SFML_HPP_ */
