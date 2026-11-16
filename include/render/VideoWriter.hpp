/*
** EPITECH PROJECT, 2024
** raytracer
** File description:
** VideoWriter
*/

#ifndef VIDEO_WRITER_HPP_
#define VIDEO_WRITER_HPP_

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace RayTracer
{

struct VideoSettings
{
    std::string path;
    std::string codec;
    int crf = 18;
    std::string preset = "medium";
    std::string bitrate;
    double fps = 24.0;
    int width = 0;
    int height = 0;
};

/** Returns the ffmpeg executable from RAYTRACER_FFMPEG or PATH, or an empty string. */
std::string findFFmpeg(void);

/** True when the path uses a supported video container extension. */
bool isVideoExtension(const std::string &path);

/** Returns the default codec for a supported video path. */
std::string defaultVideoCodec(const std::string &path);

/** Returns an empty string when the settings are valid, otherwise a message. */
std::string validateVideoSettings(const VideoSettings &settings);

/**
 * Streams RGBA frames to an ffmpeg child process. Frames are written to a
 * temporary file and renamed on finish, so a failed render never leaves a
 * final video.
 */
class VideoWriter
{
  public:
    explicit VideoWriter(const VideoSettings &settings);
    ~VideoWriter(void);

    VideoWriter(const VideoWriter &) = delete;
    VideoWriter &operator=(const VideoWriter &) = delete;

    void writeFrame(const std::vector<uint32_t> &pixels);
    void finish(void);

  private:
    struct Impl;
    std::unique_ptr<Impl> _impl;
};

} // namespace RayTracer

#endif // VIDEO_WRITER_HPP_
