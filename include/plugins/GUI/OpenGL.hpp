/*
** EPITECH PROJECT, 2024
** raytracer
** File description:
** OpenGL GUI
*/

#ifndef OPENGL_GUI_HPP_
#define OPENGL_GUI_HPP_

#include "GUI.hpp"
#include <cstddef>
#include <memory>

namespace RayTracer
{

/**
 * GLFW + OpenGL 3.3 core GUI backend.
 *
 * The renderer uploads its RGBA image into a texture; the GUI displays that
 * texture on a fullscreen triangle with letterboxing that preserves aspect
 * ratio.
 */
class OpenGLGUI : public IGUI
{
  public:
    OpenGLGUI(const std::size_t &width, const std::size_t &height);
    ~OpenGLGUI(void) override;

    void loadPPM(const std::string &path) override;
    void setImage(const std::size_t &width, const std::size_t &height,
                  const std::vector<uint32_t> &pixels) override;
    void setSize(const std::size_t &width, const std::size_t &height) override;
    void setPixels(const std::vector<std::vector<Color4D>> &pixels) override;
    void setPixel(const std::size_t &x, const std::size_t &y, const Color4D &color) override;
    void display(void) override;
    bool isOpen(void) override;
    char getEvent(void) override;
    void loop(void) override;

  private:
    struct Impl;
    std::unique_ptr<Impl> _impl;
};

} // namespace RayTracer

#endif // OPENGL_GUI_HPP_
