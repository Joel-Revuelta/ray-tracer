/*
** EPITECH PROJECT, 2024
** raytracer
** File description:
** VideoWriter
*/

#include "render/VideoWriter.hpp"
#include "render/TemporaryPath.hpp"
#include <algorithm>
#include <cctype>
#include <cerrno>
#include <cmath>
#include <csignal>
#include <cstdlib>
#include <cstring>
#include <fcntl.h>
#include <filesystem>
#include <iomanip>
#include <mutex>
#include <sstream>
#include <stdexcept>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

using namespace RayTracer;

namespace
{
using SignalHandler = void (*)(int);

std::mutex signalMutex;
int signalUsers = 0;
SignalHandler previousSigpipe = SIG_DFL;

void acquireSigpipeGuard(void)
{
    std::lock_guard<std::mutex> lock(signalMutex);

    if (signalUsers++ == 0)
        previousSigpipe = std::signal(SIGPIPE, SIG_IGN);
}

void releaseSigpipeGuard(void)
{
    std::lock_guard<std::mutex> lock(signalMutex);

    if (signalUsers > 0 && --signalUsers == 0 && previousSigpipe != SIG_ERR)
        std::signal(SIGPIPE, previousSigpipe);
}

bool setCloseOnExec(int descriptor)
{
    const int flags = fcntl(descriptor, F_GETFD);

    return flags >= 0 && fcntl(descriptor, F_SETFD, flags | FD_CLOEXEC) == 0;
}

std::string lowerExtension(const std::string &path)
{
    std::string extension = std::filesystem::path(path).extension().string();

    std::transform(extension.begin(), extension.end(), extension.begin(),
                   [](unsigned char character) { return static_cast<char>(std::tolower(character)); });
    return extension;
}

std::string lowerString(std::string value)
{
    std::transform(value.begin(), value.end(), value.begin(),
                   [](unsigned char character) { return static_cast<char>(std::tolower(character)); });
    return value;
}

bool isExecutable(const std::string &path)
{
    if (path.empty() || access(path.c_str(), X_OK) != 0)
        return false;

    std::error_code error;

    return std::filesystem::is_regular_file(path, error) && !error;
}

bool isValidPreset(const std::string &preset)
{
    static const std::vector<std::string> presets = {"ultrafast", "superfast", "veryfast", "faster", "fast",
                                                     "medium",    "slow",      "slower",   "veryslow", "placebo"};

    return std::find(presets.begin(), presets.end(), preset) != presets.end();
}

bool isValidBitrate(const std::string &bitrate)
{
    if (bitrate.empty())
        return false;

    std::size_t digits = 0;

    while (digits < bitrate.size() && std::isdigit(static_cast<unsigned char>(bitrate[digits])))
        ++digits;
    if (digits == 0)
        return false;
    if (digits == bitrate.size())
        return true;
    if (digits + 1 != bitrate.size())
        return false;

    const char suffix = static_cast<char>(std::tolower(static_cast<unsigned char>(bitrate[digits])));
    return suffix == 'k' || suffix == 'm';
}

bool isMp4Family(const std::string &extension)
{
    return extension == ".mp4" || extension == ".m4v" || extension == ".mov";
}

std::string containerFormat(const std::string &extension)
{
    if (extension == ".mp4" || extension == ".m4v")
        return "mp4";
    if (extension == ".mov")
        return "mov";
    if (extension == ".mkv")
        return "matroska";
    if (extension == ".webm")
        return "webm";
    return {};
}

std::string frameFilter(int width, int height)
{
    std::string filter;

    if (width % 2 != 0 || height % 2 != 0)
        filter = "pad=ceil(iw/2)*2:ceil(ih/2)*2,";
    filter += "scale=out_color_matrix=bt709:out_range=tv";
    return filter;
}

std::string formatFps(double fps)
{
    std::ostringstream formatted;

    formatted << std::setprecision(9) << fps;
    return formatted.str();
}

int waitForChild(pid_t pid)
{
    int status = 0;
    pid_t result;

    do
    {
        result = waitpid(pid, &status, 0);
    } while (result < 0 && errno == EINTR);

    return result < 0 ? -1 : status;
}
} // namespace

std::string RayTracer::findFFmpeg(void)
{
    const char *overridePath = std::getenv("RAYTRACER_FFMPEG");

    if (overridePath != nullptr && *overridePath != '\0')
    {
        const std::string candidate(overridePath);

        if (candidate.find('/') != std::string::npos)
            return isExecutable(candidate) ? candidate : std::string();

        const char *pathValue = std::getenv("PATH");

        if (pathValue == nullptr)
            return {};

        std::istringstream directories(pathValue);
        std::string directory;

        while (std::getline(directories, directory, ':'))
        {
            const std::string fullPath = (directory.empty() ? "." : directory) + "/" + candidate;

            if (isExecutable(fullPath))
                return fullPath;
        }
        return {};
    }

    const char *pathValue = std::getenv("PATH");

    if (pathValue == nullptr)
        return {};

    std::istringstream directories(pathValue);
    std::string directory;

    while (std::getline(directories, directory, ':'))
    {
        const std::string fullPath = (directory.empty() ? "." : directory) + "/ffmpeg";

        if (isExecutable(fullPath))
            return fullPath;
    }
    return {};
}

bool RayTracer::isVideoExtension(const std::string &path)
{
    const std::string extension = lowerExtension(path);

    return extension == ".mp4" || extension == ".m4v" || extension == ".mov" || extension == ".mkv" ||
           extension == ".webm";
}

std::string RayTracer::defaultVideoCodec(const std::string &path)
{
    return lowerExtension(path) == ".webm" ? "vp9" : "h264";
}

std::string RayTracer::validateVideoSettings(const VideoSettings &settings)
{
    const std::string extension = lowerExtension(settings.path);

    if (settings.path.empty())
        return "Video path must not be empty";
    if (!isVideoExtension(settings.path))
        return "Unsupported video extension; use .mp4, .m4v, .mov, .mkv or .webm";

    const std::string codec = lowerString(settings.codec);

    if (codec != "h264" && codec != "vp9")
        return "Video codec must be h264 or vp9";
    if (extension == ".webm" && codec != "vp9")
        return "WebM output requires the vp9 codec";
    if (isMp4Family(extension) && codec != "h264")
        return "MP4/MOV output requires the h264 codec";

    const int maximumCrf = codec == "h264" ? 51 : 63;

    if (settings.crf < 0 || settings.crf > maximumCrf)
        return "Video CRF must be between 0 and " + std::to_string(maximumCrf);

    if (codec == "h264" && !isValidPreset(lowerString(settings.preset)))
        return "Unknown H.264 preset: " + settings.preset;

    if (!settings.bitrate.empty() && !isValidBitrate(settings.bitrate))
        return "Video bitrate must look like 800k or 8M";

    if (!std::isfinite(settings.fps) || settings.fps <= 0.0)
        return "Video frame rate must be positive";
    if (settings.width <= 0 || settings.height <= 0)
        return "Video dimensions must be positive";

    return {};
}

struct VideoWriter::Impl
{
    pid_t child = -1;
    int descriptor = -1;
    bool finished = false;
    std::string finalPath;
    std::string temporaryPath;
    int width = 0;
    int height = 0;
    bool sigpipeGuard = false;

    ~Impl(void)
    {
        cleanup();
    }

    void cleanup(void) noexcept
    {
        if (descriptor >= 0)
        {
            close(descriptor);
            descriptor = -1;
        }

        if (child > 0)
        {
            waitForChild(child);
            child = -1;
        }

        if (!finished && !temporaryPath.empty())
        {
            std::error_code error;

            std::filesystem::remove(temporaryPath, error);
        }

        if (sigpipeGuard)
        {
            releaseSigpipeGuard();
            sigpipeGuard = false;
        }
    }

    int reap(void)
    {
        if (child <= 0)
            return 0;

        const int status = waitForChild(child);

        child = -1;
        return status;
    }
};

VideoWriter::VideoWriter(const VideoSettings &settings) : _impl(std::make_unique<Impl>())
{
    const std::string validationError = validateVideoSettings(settings);

    if (!validationError.empty())
        throw std::invalid_argument(validationError);

    const std::string ffmpeg = findFFmpeg();

    if (ffmpeg.empty())
        throw std::runtime_error("ffmpeg was not found; install it or set RAYTRACER_FFMPEG");

    _impl->finalPath = settings.path;
    _impl->temporaryPath = makeTemporaryPath(settings.path);
    _impl->width = settings.width;
    _impl->height = settings.height;

    std::vector<std::string> arguments;

    arguments.push_back(ffmpeg);
    arguments.push_back("-y");
    arguments.push_back("-hide_banner");
    arguments.push_back("-loglevel");
    arguments.push_back("error");
    arguments.push_back("-f");
    arguments.push_back("rawvideo");
    arguments.push_back("-pixel_format");
    arguments.push_back("rgba");
    arguments.push_back("-video_size");
    arguments.push_back(std::to_string(settings.width) + "x" + std::to_string(settings.height));
    arguments.push_back("-framerate");
    arguments.push_back(formatFps(settings.fps));
    arguments.push_back("-i");
    arguments.push_back("-");
    arguments.push_back("-an");

    const std::string codec = lowerString(settings.codec);

    if (codec == "h264")
    {
        arguments.push_back("-c:v");
        arguments.push_back("libx264");
        arguments.push_back("-preset");
        arguments.push_back(lowerString(settings.preset));
    }
    else
    {
        arguments.push_back("-c:v");
        arguments.push_back("libvpx-vp9");
        arguments.push_back("-deadline");
        arguments.push_back("good");
        arguments.push_back("-cpu-used");
        arguments.push_back("2");
    }

    if (!settings.bitrate.empty())
    {
        arguments.push_back("-b:v");
        arguments.push_back(settings.bitrate);
    }
    else
    {
        arguments.push_back("-crf");
        arguments.push_back(std::to_string(settings.crf));

        if (codec == "vp9")
        {
            arguments.push_back("-b:v");
            arguments.push_back("0");
        }
    }

    arguments.push_back("-pix_fmt");
    arguments.push_back("yuv420p");
    arguments.push_back("-vf");
    arguments.push_back(frameFilter(settings.width, settings.height));
    arguments.push_back("-colorspace");
    arguments.push_back("bt709");
    arguments.push_back("-color_primaries");
    arguments.push_back("bt709");
    arguments.push_back("-color_trc");
    arguments.push_back("bt709");
    arguments.push_back("-color_range");
    arguments.push_back("tv");

    if (isMp4Family(lowerExtension(settings.path)))
    {
        arguments.push_back("-movflags");
        arguments.push_back("+faststart");
    }

    arguments.push_back("-f");
    arguments.push_back(containerFormat(lowerExtension(settings.path)));
    arguments.push_back(_impl->temporaryPath);

    std::vector<char *> argv;

    argv.reserve(arguments.size() + 1);
    for (std::string &argument : arguments)
        argv.push_back(argument.data());
    argv.push_back(nullptr);

    int pipeDescriptors[2];

    if (pipe(pipeDescriptors) != 0)
        throw std::runtime_error("Failed to create the ffmpeg pipe: " + std::string(std::strerror(errno)));
    if (!setCloseOnExec(pipeDescriptors[0]) || !setCloseOnExec(pipeDescriptors[1]))
    {
        close(pipeDescriptors[0]);
        close(pipeDescriptors[1]);
        throw std::runtime_error("Failed to configure the ffmpeg pipe close-on-exec flag");
    }

    acquireSigpipeGuard();
    _impl->sigpipeGuard = true;

    const pid_t child = fork();

    if (child < 0)
    {
        close(pipeDescriptors[0]);
        close(pipeDescriptors[1]);
        releaseSigpipeGuard();
        _impl->sigpipeGuard = false;
        throw std::runtime_error("Failed to start ffmpeg: " + std::string(std::strerror(errno)));
    }

    if (child == 0)
    {
        if (dup2(pipeDescriptors[0], STDIN_FILENO) < 0)
            _exit(127);

        close(pipeDescriptors[0]);
        close(pipeDescriptors[1]);
        execvp(argv[0], argv.data());
        _exit(127);
    }

    close(pipeDescriptors[0]);
    _impl->child = child;
    _impl->descriptor = pipeDescriptors[1];
}

VideoWriter::~VideoWriter(void) = default;

void VideoWriter::writeFrame(const std::vector<uint32_t> &pixels)
{
    if (!_impl || _impl->finished)
        throw std::runtime_error("Cannot write to a finalized video");
    if (_impl->descriptor < 0 || _impl->child <= 0)
        throw std::runtime_error("Video writer is not running");

    const std::size_t expected = static_cast<std::size_t>(_impl->width) * static_cast<std::size_t>(_impl->height);

    if (pixels.size() < expected)
        throw std::invalid_argument("Not enough pixel data for the video frame");

    const char *data = reinterpret_cast<const char *>(pixels.data());
    std::size_t remaining = expected * sizeof(uint32_t);

    while (remaining > 0)
    {
        const ssize_t written = write(_impl->descriptor, data, remaining);

        if (written < 0)
        {
            const int writeError = errno;

            if (writeError == EINTR)
                continue;

            const int status = _impl->reap();

            if (writeError == EPIPE)
                throw std::runtime_error("ffmpeg stopped while receiving frames (exit status " +
                                         std::to_string(WIFEXITED(status) ? WEXITSTATUS(status) : -1) + ")");
            throw std::runtime_error("Failed to write a video frame: " + std::string(std::strerror(writeError)));
        }
        if (written == 0)
            throw std::runtime_error("Failed to write a video frame: ffmpeg closed the pipe");

        data += written;
        remaining -= static_cast<std::size_t>(written);
    }
}

void VideoWriter::finish(void)
{
    if (!_impl || _impl->finished)
        return;
    if (_impl->child <= 0)
    {
        std::error_code error;

        std::filesystem::remove(_impl->temporaryPath, error);
        _impl->temporaryPath.clear();
        throw std::runtime_error("Video writer is not running");
    }

    if (_impl->descriptor >= 0)
    {
        close(_impl->descriptor);
        _impl->descriptor = -1;
    }

    const int status = _impl->reap();

    if (status < 0 || !WIFEXITED(status) || WEXITSTATUS(status) != 0)
    {
        std::error_code error;

        std::filesystem::remove(_impl->temporaryPath, error);
        _impl->temporaryPath.clear();
        throw std::runtime_error("ffmpeg failed with exit status " +
                                 std::to_string(status < 0 ? -1 : (WIFEXITED(status) ? WEXITSTATUS(status) : -1)));
    }

    std::error_code error;

    std::filesystem::rename(_impl->temporaryPath, _impl->finalPath, error);
    if (error)
    {
        std::filesystem::remove(_impl->temporaryPath, error);
        _impl->temporaryPath.clear();
        throw std::runtime_error("Failed to finalize video file: " + _impl->finalPath);
    }

    _impl->temporaryPath.clear();
    _impl->finished = true;
}
