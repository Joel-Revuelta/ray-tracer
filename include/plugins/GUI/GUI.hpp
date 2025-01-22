/*
** EPITECH PROJECT, 2024
** raytracer
** File description:
** GUI
*/

#ifndef GUI_HPP_
#define GUI_HPP_

#include <Color.hpp>
#include <string>
#include <vector>

namespace RayTracer
{

class IGUI
{
  public:
    ~IGUI() = default;

    virtual void loadPPM(const std::string &path) = 0;
    virtual void setImage(const std::size_t &width, const std::size_t &height, std::vector<uint32_t> &pixels) = 0;
    virtual void setSize(const std::size_t &width, const std::size_t &height) = 0;
    virtual void setPixels(const std::vector<std::vector<Color4D>> &pixels) = 0;
    virtual void setPixel(const std::size_t &x, const std::size_t &y, const Color4D &color) = 0;
    virtual void display() = 0;
    virtual bool isOpen() = 0;
    virtual char getEvent() = 0;
    virtual void loop() = 0;
};

} // namespace RayTracer

#endif /* !GUI_HPP_ */
