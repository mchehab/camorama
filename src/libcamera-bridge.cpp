/* SPDX-License-Identifier: GPL-2.0-or-later */
#include <algorithm>
#include <atomic>
#include <cerrno>
#include <chrono>
#include <condition_variable>
#include <cstdlib>
#include <cstring>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <sys/mman.h>
#include <vector>

#include <libcamera/libcamera.h>
#include <libcamera/version.h>

#include "img_convert.h"
#include "libcamera-bridge.h"

using namespace libcamera;

#if LIBCAMERA_VERSION_MAJOR > 0 || LIBCAMERA_VERSION_MINOR >= 1
#define CAMORAMA_LIBCAMERA_MODERN_API 1
#endif

namespace {

char *copy_error(const std::string &message)
{
    return strdup(message.c_str());
}

void set_error(char **error, const std::string &message)
{
    if (error)
        *error = copy_error(message);
}

std::string error_code(const char *operation, int ret)
{
    int code = ret < 0 ? -ret : ret;

    return std::string(operation) + ": " + std::strerror(code);
}

uint32_t camorama_pixel_format(const PixelFormat &format)
{
    /* libcamera names packed RGB formats by bit significance. */
    if (format == formats::BGR888)
        return V4L2_PIX_FMT_RGB24;
    if (format == formats::RGB888)
        return V4L2_PIX_FMT_BGR24;

    return format.fourcc();
}

enum class FormatRank {
    MJPEG,
    H264,
    RGB,
    RAW,
    UNSUPPORTED,
};

FormatRank format_rank(uint32_t format)
{
    if (format == V4L2_PIX_FMT_MJPEG)
        return FormatRank::MJPEG;
    if (format == V4L2_PIX_FMT_H264)
        return FormatRank::H264;
    if (format == V4L2_PIX_FMT_RGB24 || format == V4L2_PIX_FMT_BGR24)
        return FormatRank::RGB;
    return FormatRank::RAW;
}

bool single_plane_format(uint32_t format)
{
    switch (format) {
    case V4L2_PIX_FMT_NV12:
    case V4L2_PIX_FMT_NV21:
    case V4L2_PIX_FMT_NV16:
    case V4L2_PIX_FMT_NV61:
    case V4L2_PIX_FMT_YUV420:
    case V4L2_PIX_FMT_YVU420:
    case V4L2_PIX_FMT_YUV422P:
        return false;
    default:
        return true;
    }
}

struct Mapping {
    void *base = MAP_FAILED;
    const unsigned char *data = nullptr;
    size_t mapped_length = 0;
};

} /* namespace */

struct libcamera_bridge {
    explicit libcamera_bridge(bool debug_mode)
        : debug(debug_mode)
    {
    }

    ~libcamera_bridge()
    {
        stopCapture();
        if (camera) {
            if (acquired)
                camera->release();
            camera.reset();
        }
        if (manager_started)
            manager.stop();
    }

    int open(const char *selector, std::string &error)
    {
        int ret = manager.start();
        if (ret) {
            error = error_code("failed to start camera manager", ret);
            return ret;
        }
        manager_started = true;

        const auto cameras = manager.cameras();
        if (cameras.empty()) {
            error = "no cameras were found";
            return -ENODEV;
        }

        if (selector && *selector) {
            char *end = nullptr;
            unsigned long index = std::strtoul(selector, &end, 10);

            for (const auto &candidate : cameras) {
#ifdef CAMORAMA_LIBCAMERA_MODERN_API
                if (candidate->id() == selector) {
#else
                if (candidate->name() == selector) {
#endif
                    camera = candidate;
                    break;
                }
            }
            if (!camera && end && !*end && index < cameras.size())
                camera = cameras[index];
            if (!camera) {
                error = std::string("camera '") + selector + "' was not found";
                return -ENODEV;
            }
        } else {
            camera = cameras.front();
        }

#ifdef CAMORAMA_LIBCAMERA_MODERN_API
        id = camera->id();
#else
        id = camera->name();
#endif
        name = id;

        ret = camera->acquire();
        if (ret) {
            error = error_code("failed to acquire camera", ret);
            return ret;
        }
        acquired = true;

        auto config = camera->generateConfiguration({ StreamRole::Viewfinder });
        if (!config || config->empty()) {
            error = "camera has no viewfinder stream";
            return -EINVAL;
        }

        const StreamFormats &formats = config->at(0).formats();
        const auto pixel_formats = formats.pixelformats();

        FormatRank best_rank = FormatRank::UNSUPPORTED;
        for (const PixelFormat &candidate : pixel_formats) {
            uint32_t format = camorama_pixel_format(candidate);
            FormatRank rank = format_rank(format);

            if (!img_format_get(format) || !single_plane_format(format) ||
                rank >= best_rank)
                continue;
            pixel_format = candidate;
            v4l2_pixel_format = format;
            best_rank = rank;
        }
        if (!pixel_format.isValid()) {
            error = "camera cannot provide a supported viewfinder stream";
            return -ENOTSUP;
        }

        sizes = formats.sizes(pixel_format);
        if (sizes.empty()) {
            const SizeRange range = formats.range(pixel_format);
            for (unsigned int i = 0; i <= 4; i++) {
                unsigned int width = range.min.width +
                    i * (range.max.width - range.min.width) / 4;
                unsigned int height = range.min.height +
                    i * (range.max.height - range.min.height) / 4;

                if (range.hStep > 1)
                    width -= (width - range.min.width) % range.hStep;
                if (range.vStep > 1)
                    height -= (height - range.min.height) % range.vStep;
                sizes.emplace_back(width, height);
            }
        }
        std::sort(sizes.begin(), sizes.end(),
                  [](const Size &a, const Size &b) {
                      if (a.width != b.width)
                          return a.width > b.width;
                      return a.height > b.height;
                  });
        sizes.erase(std::unique(sizes.begin(), sizes.end()), sizes.end());

        return 0;
    }

    Size nearestSize(unsigned int requested_width,
                     unsigned int requested_height) const
    {
        auto best = sizes.front();
        uint64_t best_distance = UINT64_MAX;

        for (const Size &candidate : sizes) {
            int64_t dx = static_cast<int64_t>(candidate.width) - requested_width;
            int64_t dy = static_cast<int64_t>(candidate.height) - requested_height;
            uint64_t distance = dx * dx + dy * dy;

            if (distance < best_distance) {
                best = candidate;
                best_distance = distance;
            }
        }
        return best;
    }

    int configure(unsigned int &requested_width,
                  unsigned int &requested_height,
                  unsigned int &output_stride,
                  unsigned int &output_frame_size,
                  unsigned int &output_pixformat, std::string &error)
    {
        stopCapture();
        configured = false;

        auto config = camera->generateConfiguration({ StreamRole::Viewfinder });
        if (!config || config->empty()) {
            error = "camera has no viewfinder stream";
            return -EINVAL;
        }

        Size selected = nearestSize(requested_width, requested_height);
        StreamConfiguration &stream_config = config->at(0);
        stream_config.pixelFormat = pixel_format;
        stream_config.size = selected;
        stream_config.bufferCount = std::max(stream_config.bufferCount, 4U);

        CameraConfiguration::Status status = config->validate();
        if (status == CameraConfiguration::Invalid) {
            error = "requested stream configuration is invalid";
            return -EINVAL;
        }
        if (stream_config.pixelFormat != pixel_format) {
            error = "camera adjusted the stream to a non-RGB format";
            return -ENOTSUP;
        }

        int ret = camera->configure(config.get());
        if (ret) {
            error = error_code("failed to configure camera", ret);
            return ret;
        }

        stream = stream_config.stream();
        width = stream_config.size.width;
        height = stream_config.size.height;
        stride = stream_config.stride;
        frame_size = stream_config.frameSize;
        requested_width = width;
        requested_height = height;
        output_stride = stride;
        output_frame_size = frame_size;
        output_pixformat = v4l2_pixel_format;
        configured = true;

        return 0;
    }

    int startCapture(std::string &error)
    {
        if (!configured) {
            error = "camera is not configured";
            return -EINVAL;
        }
        if (started)
            return 0;

        allocator = std::make_unique<FrameBufferAllocator>(camera);
        int ret = allocator->allocate(stream);
        if (ret < 0) {
            error = error_code("failed to allocate capture buffers", ret);
            allocator.reset();
            return ret;
        }

        for (const auto &buffer : allocator->buffers(stream)) {
            const FrameBuffer::Plane &plane = buffer->planes().front();
#ifdef CAMORAMA_LIBCAMERA_MODERN_API
            int fd = plane.fd.get();
            off_t offset = plane.offset;
#else
            int fd = plane.fd.fd();
            off_t offset = 0;
#endif
            size_t mapped_length = offset + plane.length;
            void *base = mmap(nullptr, mapped_length, PROT_READ, MAP_SHARED,
                              fd, 0);
            if (base == MAP_FAILED) {
                error = error_code("failed to map capture buffer", errno);
                releaseBuffers();
                return -errno;
            }
            if (plane.length < frame_size) {
                munmap(base, mapped_length);
                error = "capture buffer is smaller than the configured image";
                releaseBuffers();
                return -EIO;
            }
            mappings[buffer.get()] = {
                base,
                static_cast<const unsigned char *>(base) + offset,
                mapped_length,
            };

#ifdef CAMORAMA_LIBCAMERA_MODERN_API
            std::unique_ptr<Request> request = camera->createRequest();
#else
            std::unique_ptr<Request> request(camera->createRequest());
#endif
            if (!request) {
                error = "failed to create capture request";
                releaseBuffers();
                return -ENOMEM;
            }
            ret = request->addBuffer(stream, buffer.get());
            if (ret) {
                error = error_code("failed to attach capture buffer", ret);
                releaseBuffers();
                return ret;
            }
            requests.push_back(std::move(request));
        }

        camera->requestCompleted.connect(this,
                                          &libcamera_bridge::requestComplete);
        {
            std::lock_guard<std::mutex> guard(frame_mutex);
            latest_frame.clear();
            delivered_generation = frame_generation;
        }
        running = true;
        ret = camera->start();
        if (ret) {
            running = false;
            camera->requestCompleted.disconnect(this);
            error = error_code("failed to start capture", ret);
            releaseBuffers();
            return ret;
        }
        started = true;

        for (const auto &request : requests) {
            ret = camera->queueRequest(request.get());
            if (ret) {
                error = error_code("failed to queue capture request", ret);
                stopCapture();
                return ret;
            }
        }
        return 0;
    }

    void requestComplete(Request *request)
    {
        if (request->status() == Request::RequestCancelled)
            return;

        FrameBuffer *buffer = request->findBuffer(stream);
        auto mapping = mappings.find(buffer);
        if (request->status() == Request::RequestComplete &&
            buffer && mapping != mappings.end() &&
            buffer->metadata().status == FrameMetadata::FrameSuccess) {
            const unsigned char *source = mapping->second.data;
            size_t bytes_used =
                buffer->metadata().planes().front().bytesused;

            if (!bytes_used || bytes_used > buffer->planes().front().length)
                bytes_used = frame_size;
            std::vector<unsigned char> frame(source, source + bytes_used);

            {
                std::lock_guard<std::mutex> guard(frame_mutex);
                latest_frame.swap(frame);
                frame_generation++;
            }
            frame_ready.notify_one();
        }

        if (!running)
            return;
#ifdef CAMORAMA_LIBCAMERA_MODERN_API
        request->reuse(Request::ReuseBuffers);
#endif
        if (camera->queueRequest(request)) {
            running = false;
            frame_ready.notify_all();
        }
    }

    int readFrame(unsigned char *output, size_t output_size,
                  unsigned int timeout_ms, size_t &bytes_used,
                  std::string &error)
    {
        std::unique_lock<std::mutex> lock(frame_mutex);

        const uint64_t previous = delivered_generation;
        if (!frame_ready.wait_for(lock, std::chrono::milliseconds(timeout_ms),
                                  [&] { return frame_generation != previous ||
                                               !running; })) {
            error = "timed out waiting for a frame";
            return -ETIMEDOUT;
        }
        if (!running || latest_frame.empty()) {
            error = "capture stopped while waiting for a frame";
            return -EPIPE;
        }
        if (output_size < latest_frame.size()) {
            error = "output buffer is too small";
            return -ENOSPC;
        }

        std::memcpy(output, latest_frame.data(), latest_frame.size());
        bytes_used = latest_frame.size();
        delivered_generation = frame_generation;
        return 0;
    }

    void stopCapture()
    {
        if (started) {
            running = false;
            frame_ready.notify_all();
            camera->stop();
            camera->requestCompleted.disconnect(this);
            started = false;
        }
        releaseBuffers();
    }

    void releaseBuffers()
    {
        requests.clear();
        for (const auto &entry : mappings)
            if (entry.second.base != MAP_FAILED)
                munmap(entry.second.base, entry.second.mapped_length);
        mappings.clear();
        allocator.reset();
    }

    bool debug = false;
    CameraManager manager;
    bool manager_started = false;
    std::shared_ptr<Camera> camera;
    bool acquired = false;
    std::string id;
    std::string name;
    PixelFormat pixel_format;
    uint32_t v4l2_pixel_format = 0;
    std::vector<Size> sizes;
    Stream *stream = nullptr;
    unsigned int width = 0;
    unsigned int height = 0;
    unsigned int stride = 0;
    unsigned int frame_size = 0;
    bool configured = false;
    bool started = false;
    std::atomic<bool> running{ false };
    std::unique_ptr<FrameBufferAllocator> allocator;
    std::vector<std::unique_ptr<Request>> requests;
    std::map<FrameBuffer *, Mapping> mappings;
    std::mutex frame_mutex;
    std::condition_variable frame_ready;
    std::vector<unsigned char> latest_frame;
    uint64_t frame_generation = 0;
    uint64_t delivered_generation = 0;
};

extern "C" {

libcamera_bridge_t *libcamera_bridge_create(const char *camera_id, int debug,
                                             char **error)
{
    std::unique_ptr<libcamera_bridge> bridge(new libcamera_bridge(debug));
    std::string message;

    if (bridge->open(camera_id, message)) {
        set_error(error, message);
        return nullptr;
    }
    return bridge.release();
}

void libcamera_bridge_destroy(libcamera_bridge_t *bridge)
{
    delete bridge;
}

const char *libcamera_bridge_camera_id(const libcamera_bridge_t *bridge)
{
    return bridge->id.c_str();
}

const char *libcamera_bridge_camera_name(const libcamera_bridge_t *bridge)
{
    return bridge->name.c_str();
}

unsigned int libcamera_bridge_pixel_format(const libcamera_bridge_t *bridge)
{
    return bridge->v4l2_pixel_format;
}

unsigned int libcamera_bridge_num_sizes(const libcamera_bridge_t *bridge)
{
    return bridge->sizes.size();
}

int libcamera_bridge_get_size(const libcamera_bridge_t *bridge,
                              unsigned int index, unsigned int *width,
                              unsigned int *height)
{
    if (index >= bridge->sizes.size())
        return -EINVAL;
    *width = bridge->sizes[index].width;
    *height = bridge->sizes[index].height;
    return 0;
}

void libcamera_bridge_try_size(const libcamera_bridge_t *bridge,
                               unsigned int *width, unsigned int *height)
{
    const Size size = bridge->nearestSize(*width, *height);
    *width = size.width;
    *height = size.height;
}

int libcamera_bridge_configure(libcamera_bridge_t *bridge,
                               unsigned int *width, unsigned int *height,
                               unsigned int *stride,
                               unsigned int *frame_size,
                               unsigned int *pixformat, char **error)
{
    std::string message;
    int ret = bridge->configure(*width, *height, *stride, *frame_size,
                                *pixformat, message);
    if (ret)
        set_error(error, message);
    return ret;
}

int libcamera_bridge_start(libcamera_bridge_t *bridge, char **error)
{
    std::string message;
    int ret = bridge->startCapture(message);
    if (ret)
        set_error(error, message);
    return ret;
}

void libcamera_bridge_stop(libcamera_bridge_t *bridge)
{
    bridge->stopCapture();
}

int libcamera_bridge_read(libcamera_bridge_t *bridge, unsigned char *output,
                          size_t output_size, unsigned int timeout_ms,
                          size_t *bytes_used, char **error)
{
    std::string message;
    int ret = bridge->readFrame(output, output_size, timeout_ms,
                                *bytes_used, message);
    if (ret)
        set_error(error, message);
    return ret;
}

void libcamera_bridge_free_string(char *string)
{
    free(string);
}

} /* extern \"C\" */
