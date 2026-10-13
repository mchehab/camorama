/* SPDX-License-Identifier: GPL-2.0-or-later */

/* libcamera C++ implementation and Camorama backend adapter. */

#include <algorithm>
#include <array>
#include <atomic>
#include <cerrno>
#include <chrono>
#include <condition_variable>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <sys/mman.h>
#include <utility>
#include <vector>

#include <libcamera/libcamera.h>
#include <libcamera/property_ids.h>
#include <libcamera/version.h>

extern "C" {
#include <linux/videodev2.h>

#include "camera-backend.h"
#include "img_convert.h"
#include "support.h"

#include <glib/gi18n.h>
#include <config.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "libcamera.h"
}

#if LIBCAMERA_VERSION_MAJOR > 0 || LIBCAMERA_VERSION_MINOR >= 5
#define CAMORAMA_LIBCAMERA_HAS_EXPOSURE_GAIN_MODE 1
#endif

using namespace libcamera;

enum libcamera_control_type {
	LIBCAMERA_CONTROL_BOOL,
	LIBCAMERA_CONTROL_INTEGER,
	LIBCAMERA_CONTROL_INTEGER64,
	LIBCAMERA_CONTROL_FLOAT,
	LIBCAMERA_CONTROL_MENU,
};

/*
 * As camorama uses V4L2 internally, we need to have a map between
 * libcamera's internal representation and V4L2.
 *
 * This table is in sync with libcamera version 0.3.2.
 */
static const std::map<PixelFormat, uint32_t> v4l2Formats = {
    { formats::RGB565, V4L2_PIX_FMT_RGB565 },
    { formats::RGB565_BE, V4L2_PIX_FMT_RGB565X },
    { formats::BGR888, V4L2_PIX_FMT_RGB24 },
    { formats::RGB888, V4L2_PIX_FMT_BGR24 },
    { formats::XRGB8888, V4L2_PIX_FMT_XBGR32 },
    { formats::XBGR8888, V4L2_PIX_FMT_RGBX32 },
    { formats::RGBX8888, V4L2_PIX_FMT_BGRX32 },
    { formats::BGRX8888, V4L2_PIX_FMT_XRGB32 },
    { formats::ABGR8888, V4L2_PIX_FMT_RGBA32 },
    { formats::ARGB8888, V4L2_PIX_FMT_ABGR32 },
    { formats::BGRA8888, V4L2_PIX_FMT_ARGB32 },
    { formats::RGBA8888, V4L2_PIX_FMT_BGRA32 },
    { formats::BGR161616, V4L2_PIX_FMT_RGB48 },
    { formats::RGB161616, V4L2_PIX_FMT_BGR48 },
    { formats::YUYV, V4L2_PIX_FMT_YUYV },
    { formats::YVYU, V4L2_PIX_FMT_YVYU },
    { formats::UYVY, V4L2_PIX_FMT_UYVY },
    { formats::VYUY, V4L2_PIX_FMT_VYUY },
    { formats::AVUY8888, V4L2_PIX_FMT_YUVA32 },
    { formats::XVUY8888, V4L2_PIX_FMT_YUVX32 },
    { formats::NV12, V4L2_PIX_FMT_NV12 },
    { formats::NV21, V4L2_PIX_FMT_NV21 },
    { formats::NV16, V4L2_PIX_FMT_NV16 },
    { formats::NV61, V4L2_PIX_FMT_NV61 },
    { formats::NV24, V4L2_PIX_FMT_NV24 },
    { formats::NV42, V4L2_PIX_FMT_NV42 },
    { formats::YUV420, V4L2_PIX_FMT_YUV420 },
    { formats::YVU420, V4L2_PIX_FMT_YVU420 },
    { formats::YUV422, V4L2_PIX_FMT_YUV422P },
    { formats::YVU422, V4L2_PIX_FMT_YVU422M },
    { formats::YUV444, V4L2_PIX_FMT_YUV444M },
    { formats::YVU444, V4L2_PIX_FMT_YVU444M },
    { formats::R8, V4L2_PIX_FMT_GREY },
    { formats::R10, V4L2_PIX_FMT_Y10 },
    { formats::R10_CSI2P, V4L2_PIX_FMT_Y10P },
    { formats::R12_CSI2P, V4L2_PIX_FMT_Y12P },
    { formats::R12, V4L2_PIX_FMT_Y12 },
    { formats::R16, V4L2_PIX_FMT_Y16 },
    { formats::MONO_PISP_COMP1, V4L2_PIX_FMT_PISP_COMP1_MONO },
    { formats::SBGGR8, V4L2_PIX_FMT_SBGGR8 },
    { formats::SGBRG8, V4L2_PIX_FMT_SGBRG8 },
    { formats::SGRBG8, V4L2_PIX_FMT_SGRBG8 },
    { formats::SRGGB8, V4L2_PIX_FMT_SRGGB8 },
    { formats::SBGGR10, V4L2_PIX_FMT_SBGGR10 },
    { formats::SGBRG10, V4L2_PIX_FMT_SGBRG10 },
    { formats::SGRBG10, V4L2_PIX_FMT_SGRBG10 },
    { formats::SRGGB10, V4L2_PIX_FMT_SRGGB10 },
    { formats::SBGGR10_CSI2P, V4L2_PIX_FMT_SBGGR10P },
    { formats::SGBRG10_CSI2P, V4L2_PIX_FMT_SGBRG10P },
    { formats::SGRBG10_CSI2P, V4L2_PIX_FMT_SGRBG10P },
    { formats::SRGGB10_CSI2P, V4L2_PIX_FMT_SRGGB10P },
    { formats::SBGGR12, V4L2_PIX_FMT_SBGGR12 },
    { formats::SGBRG12, V4L2_PIX_FMT_SGBRG12 },
    { formats::SGRBG12, V4L2_PIX_FMT_SGRBG12 },
    { formats::SRGGB12, V4L2_PIX_FMT_SRGGB12 },
    { formats::SBGGR12_CSI2P, V4L2_PIX_FMT_SBGGR12P },
    { formats::SGBRG12_CSI2P, V4L2_PIX_FMT_SGBRG12P },
    { formats::SGRBG12_CSI2P, V4L2_PIX_FMT_SGRBG12P },
    { formats::SRGGB12_CSI2P, V4L2_PIX_FMT_SRGGB12P },
    { formats::SBGGR14, V4L2_PIX_FMT_SBGGR14 },
    { formats::SGBRG14, V4L2_PIX_FMT_SGBRG14 },
    { formats::SGRBG14, V4L2_PIX_FMT_SGRBG14 },
    { formats::SRGGB14, V4L2_PIX_FMT_SRGGB14 },
    { formats::SBGGR14_CSI2P, V4L2_PIX_FMT_SBGGR14P },
    { formats::SGBRG14_CSI2P, V4L2_PIX_FMT_SGBRG14P },
    { formats::SGRBG14_CSI2P, V4L2_PIX_FMT_SGRBG14P },
    { formats::SRGGB14_CSI2P, V4L2_PIX_FMT_SRGGB14P },
    { formats::SBGGR16, V4L2_PIX_FMT_SBGGR16 },
    { formats::SGBRG16, V4L2_PIX_FMT_SGBRG16 },
    { formats::SGRBG16, V4L2_PIX_FMT_SGRBG16 },
    { formats::SRGGB16, V4L2_PIX_FMT_SRGGB16 },
    { formats::SBGGR10_IPU3, V4L2_PIX_FMT_IPU3_SBGGR10 },
    { formats::SGBRG10_IPU3, V4L2_PIX_FMT_IPU3_SGBRG10 },
    { formats::SGRBG10_IPU3, V4L2_PIX_FMT_IPU3_SGRBG10 },
    { formats::SRGGB10_IPU3, V4L2_PIX_FMT_IPU3_SRGGB10 },
    { formats::BGGR_PISP_COMP1, V4L2_PIX_FMT_PISP_COMP1_BGGR },
    { formats::GBRG_PISP_COMP1, V4L2_PIX_FMT_PISP_COMP1_GBRG },
    { formats::GRBG_PISP_COMP1, V4L2_PIX_FMT_PISP_COMP1_GRBG },
    { formats::RGGB_PISP_COMP1, V4L2_PIX_FMT_PISP_COMP1_RGGB },
    { formats::MJPEG, V4L2_PIX_FMT_MJPEG },
};

static uint32_t libcamera_2_v4l2_fourcc(const PixelFormat &format)
{
    const auto it = v4l2Formats.find(format);
    if (it != v4l2Formats.end())
        return it->second;

    return format.fourcc();
}

static std::string camera_model(const std::shared_ptr<Camera> &camera,
                                const std::string &fallback)
{
    const auto model = camera->properties().get(properties::Model);

    if (model && !model->empty())
        return std::string(*model);

    return fallback;
}

/*
 * img_convert doesn't support multiplane formats. Instead, assumes a single
 * V4L2 streaming plane, internally splitting on two or tree planes.
 *
 * So, at least for now, those standards should be ignored.
 */
static bool single_plane_format(uint32_t format)
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

/*
 * Opaque struct used by public methods
 */
struct libcamera_bridge {
    struct FormatSupport {
        PixelFormat pixel_format;
        uint32_t v4l2_pixel_format;
        unsigned int order;
        bool compressed;
        std::vector<Size> sizes;
    };

    explicit libcamera_bridge(bool debug_mode) : debug(debug_mode)
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

    void open(const char *selector)
    {
        int ret = manager.start();
        if (ret) {
            g_error(_("could not start camera manager: %s"),
                    std::strerror(-ret));
        }
        manager_started = true;

        const auto cameras = manager.cameras();
        if (cameras.empty()) {
            g_error(_("no cameras were found"));
        }

        if (selector && *selector) {
            char *end = nullptr;
            unsigned long index = std::strtoul(selector, &end, 10);

            for (const auto &candidate : cameras) {
                if (candidate->id() == selector) {
                    camera = candidate;
                    break;
                }
            }
            if (!camera && end && !*end && index < cameras.size())
                camera = cameras[index];
            if (!camera) {
                g_error(_("camera '%s' was not found"), selector);
            }
        } else {
            camera = cameras.front();
        }

        id = camera->id();
        name = id;

        ret = camera->acquire();
        if (ret) {
            g_error(_("failed to acquire camera: %s"), std::strerror(-ret));
        }
        acquired = true;

        auto config = camera->generateConfiguration({ StreamRole::Viewfinder });
        if (!config || config->empty()) {
            g_error(_("camera has no viewfinder stream"));
        }

        const StreamFormats &stream_formats = config->at(0).formats();
        const auto pixel_formats = stream_formats.pixelformats();

        format_supports.clear();
        for (const PixelFormat &candidate : pixel_formats) {
            uint32_t format = libcamera_2_v4l2_fourcc(candidate);
            unsigned int order = img_format_order(format);

            if (!img_format_get(format) || !single_plane_format(format) ||
                order >= supported_formats_count)
                continue;

            /*
             * I didn't find a way to get frameintervals using libcamera.
             * So, do the next best thing to support high resolution
             * USB 2.0 high speed cameras: prioritize compressed algorithms
             * (currently, only MJPEG).
             *
             * Without that, 1920x1080 at 30fps won't work: it would fallback
             * to 5fps (on my tests with a Logitech C920).
             */

            FormatSupport support = {
                candidate,
                format,
                order,
                format == V4L2_PIX_FMT_MJPEG || format == V4L2_PIX_FMT_JPEG,
                stream_formats.sizes(candidate),
            };

            if (support.sizes.empty()) {
                const SizeRange range = stream_formats.range(candidate);
                for (unsigned int i = 0; i <= 4; i++) {
                    unsigned int width = range.min.width +
                                         i * (range.max.width - range.min.width) / 4;
                    unsigned int height = range.min.height +
                                          i * (range.max.height - range.min.height) / 4;

                    if (range.hStep > 1)
                        width -= (width - range.min.width) % range.hStep;

                    if (range.vStep > 1)
                        height -= (height - range.min.height) % range.vStep;

                    support.sizes.emplace_back(width, height);
                }
            }
            format_supports.push_back(std::move(support));
        }

        if (format_supports.empty()) {
            g_error(_("camera cannot provide a supported viewfinder stream"));
        }

        std::stable_sort(format_supports.begin(), format_supports.end(),
                         [](const FormatSupport &a, const FormatSupport &b)
        {
            if (a.compressed != b.compressed)
                return a.compressed;
            if (a.compressed && a.v4l2_pixel_format != b.v4l2_pixel_format)
                return a.v4l2_pixel_format == V4L2_PIX_FMT_MJPEG;
            return a.order < b.order;
        });

        sizes.clear();

        for (const FormatSupport &support : format_supports)
             sizes.insert(sizes.end(), support.sizes.begin(), support.sizes.end());
        std::sort(sizes.begin(), sizes.end(), [](const Size &a, const Size &b)
        {
            if (a.width != b.width)
                return a.width > b.width;
            return a.height > b.height;
        });

        sizes.erase(std::unique(sizes.begin(), sizes.end()), sizes.end());
        if (sizes.empty()) {
            g_error(_("camera has no supported viewfinder resolutions"));
        }

        const FormatSupport *default_format = formatForSize(sizes.front());
        if (!default_format) {
            g_error(_("camera has no supported format for its resolutions"));
        }
        pixel_format = default_format->pixel_format;
        v4l2_pixel_format = default_format->v4l2_pixel_format;

    }

    const FormatSupport *formatForSize(const Size &size) const
    {
        for (const FormatSupport &support : format_supports) {
            if (std::any_of(support.sizes.begin(), support.sizes.end(), [&size](const Size &supported)
            {
                return supported.width == size.width &&
                    supported.height == size.height;
            }))
            return &support;
        }
        return nullptr;
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

    void configure(unsigned int &requested_width,
                   unsigned int &requested_height,
                   unsigned int &output_stride,
                   unsigned int &output_frame_size,
                   unsigned int &output_pixformat)
    {
        stopCapture();
        configured = false;

        auto config = camera->generateConfiguration({ StreamRole::Viewfinder });
        if (!config || config->empty()) {
            g_error(_("camera has no viewfinder stream"));
        }

        Size selected = nearestSize(requested_width, requested_height);
        const FormatSupport *selected_format = formatForSize(selected);
        if (!selected_format) {
            g_error(_("camera has no supported format for the selected resolution"));
        }
        StreamConfiguration &stream_config = config->at(0);
        stream_config.pixelFormat = selected_format->pixel_format;
        stream_config.size = selected;
        stream_config.bufferCount = std::max(stream_config.bufferCount, 4U);

        CameraConfiguration::Status status = config->validate();
        if (status == CameraConfiguration::Invalid) {
            g_error(_("requested stream configuration is invalid"));
        }
        if (stream_config.pixelFormat != selected_format->pixel_format) {
            g_error(_("camera adjusted the stream to a non-RGB format"));
        }

        int ret = camera->configure(config.get());
        if (ret) {
            g_error(_("failed to configure camera: %s"), std::strerror(-ret));
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
        output_pixformat = selected_format->v4l2_pixel_format;
        configured = true;

    }

    void startCapture()
    {
        if (!configured) {
            g_error(_("camera is not configured"));
        }
        if (started)
            return;

        allocator = std::make_unique<FrameBufferAllocator>(camera);

        int ret = allocator->allocate(stream);
        if (ret < 0) {
            g_error(_("failed to allocate capture buffers: %s"),
                    std::strerror(-ret));
            allocator.reset();
        }

        for (const auto &buffer : allocator->buffers(stream)) {
            const FrameBuffer::Plane &plane = buffer->planes().front();
            int fd = plane.fd.get();
            off_t offset = plane.offset;
            size_t mapped_length = offset + plane.length;
            void *base = mmap(nullptr, mapped_length, PROT_READ, MAP_SHARED,
                            fd, 0);
            if (base == MAP_FAILED) {
                int saved_errno = errno;
                releaseBuffers();
                g_error(_("failed to map capture buffer: %s"),
                        std::strerror(saved_errno));
            }
            if (plane.length < frame_size) {
                munmap(base, mapped_length);
                releaseBuffers();
                g_error(_("capture buffer is smaller than the configured image"));
            }
            mappings[buffer.get()] = {
                base,
                static_cast<const unsigned char *>(base) + offset,
                mapped_length,
            };

            std::unique_ptr<Request> request = camera->createRequest();

            if (!request) {
                releaseBuffers();
                g_error(_("failed to create capture request"));
            }

            ret = request->addBuffer(stream, buffer.get());
            if (ret) {
                releaseBuffers();
                g_error(_("failed to attach capture buffer: %s"),
                        std::strerror(-ret));
            }
            applyControls(request.get());
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
            releaseBuffers();
            g_error(_("failed to start capture: %s"), std::strerror(-ret));
        }
        started = true;

        for (const auto &request : requests) {
            ret = camera->queueRequest(request.get());
            if (ret) {
                stopCapture();
                g_error(_("failed to queue capture request: %s"),
                        std::strerror(-ret));
            }
        }
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
        request->reuse(Request::ReuseBuffers);
        applyControls(request);
        if (camera->queueRequest(request)) {
            running = false;
            frame_ready.notify_all();
        }
    }

    int readFrame(unsigned char *output, size_t output_size,
                  unsigned int timeout_ms, size_t &bytes_used)
    {
        std::unique_lock<std::mutex> lock(frame_mutex);

        const uint64_t previous = delivered_generation;
        if (!frame_ready.wait_for(lock, std::chrono::milliseconds(timeout_ms),
                                [&] { return frame_generation != previous ||
                                            !running; })) {
            g_warning(_("timed out waiting for a frame"));
            return -ETIMEDOUT;
        }
        if (!running || latest_frame.empty()) {
            g_warning(_("capture stopped while waiting for a frame"));
            return -EPIPE;
        }
        if (output_size < latest_frame.size()) {
            g_warning(_("output buffer is too small"));
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

    static uint32_t control_to_v4l(const ControlId *control)
    {
        if (control == &controls::Brightness)
            return V4L2_CID_BRIGHTNESS;
        if (control == &controls::Contrast)
            return V4L2_CID_CONTRAST;
        if (control == &controls::Saturation)
            return V4L2_CID_SATURATION;
        if (control == &controls::Sharpness)
            return V4L2_CID_SHARPNESS;
        if (control == &controls::AnalogueGain)
            return V4L2_CID_GAIN;
        if (control == &controls::ExposureTime)
            return V4L2_CID_EXPOSURE_ABSOLUTE;
        if (control == &controls::Gamma)
            return V4L2_CID_GAMMA;
#ifdef CAMORAMA_LIBCAMERA_HAS_EXPOSURE_GAIN_MODE
        if (control == &controls::AeEnable)
            return V4L2_CID_EXPOSURE_AUTO_PRIORITY;
        if (control == &controls::ExposureTimeMode)
            return V4L2_CID_EXPOSURE_AUTO;
        if (control == &controls::AnalogueGainMode)
            return V4L2_CID_AUTOGAIN;
#else
        if (control == &controls::AeEnable)
            return V4L2_CID_EXPOSURE_AUTO;
#endif
        return 0;
    }

    /* Imported a simplified version from v4l2_ctrl_get_name() */
    static const char *control_to_name(const ControlId *control)
    {
        switch (control_to_v4l(control)) {
        case V4L2_CID_BRIGHTNESS:
            return _("Brightness");
        case V4L2_CID_CONTRAST:
            return _("Contrast");
        case V4L2_CID_SATURATION:
            return _("Saturation");
        case V4L2_CID_SHARPNESS:
            return _("Sharpness");
        case V4L2_CID_HUE:
            return _("Hue");
        case V4L2_CID_GAMMA:
            return _("Gamma");
        case V4L2_CID_EXPOSURE:
            return _("Exposure");
        case V4L2_CID_AUTOGAIN:
            return _("Gain, Automatic");
        case V4L2_CID_GAIN:
            return _("Gain");
        case V4L2_CID_EXPOSURE_AUTO:
            return _("Auto Exposure");
        case V4L2_CID_EXPOSURE_ABSOLUTE:
            return _("Exposure Time, Absolute");
        case V4L2_CID_EXPOSURE_AUTO_PRIORITY:
            return _("Exposure, Dynamic Framerate");
        default:
            return control->name().c_str();
        }
    }

    const ControlId *find_control(uint32_t id) const
    {
        for (const auto &entry : camera->controls())
            if (control_to_v4l(entry.first) == id)
                return entry.first;
        return nullptr;
    }

    static bool scalar_control(ControlType type)
    {
        return type == ControlTypeBool || type == ControlTypeInteger32 ||
            type == ControlTypeInteger64 || type == ControlTypeFloat;
    }

    ControlValue control_value(const ControlId *control, int32_t value) const
    {
        switch (control->type()) {
        case ControlTypeBool:
            return ControlValue(!!value);
        case ControlTypeInteger64:
            return ControlValue(static_cast<int64_t>(value));
        case ControlTypeFloat:
            return ControlValue(value / 1000.0f);
        default:
            return ControlValue(value);
        }
    }

    void applyControls(Request *request)
    {
        std::lock_guard<std::mutex> guard(control_mutex);

        if (frame_duration_us && supports_frame_duration()) {
            std::array<int64_t, 2> limits = {
                static_cast<int64_t>(frame_duration_us),
                static_cast<int64_t>(frame_duration_us),
            };
            request->controls().set(controls::FrameDurationLimits,
                                    Span<const int64_t, 2>(limits));
        }
        std::map<uint32_t, int32_t> pending;
        pending.swap(pending_control_values);
        for (const auto &entry : pending) {
            const ControlId *control = find_control(entry.first);

            /* The V4L2 id map is shared by several pipelines.  A mapped id
            * is only valid if this camera actually advertises it. */
            if (!control)
                continue;
            request->controls().set(control->id(),
                                    control_value(control, entry.second));
        }
    }

    bool supports_frame_duration() const
    {
        return camera->controls().find(&controls::FrameDurationLimits) !=
            camera->controls().end();
    }

    int controlInfo(uint32_t id, int32_t &min, int32_t &max,
                    int32_t &def, int32_t &step, const char *&name) const
    {
        const ControlId *control = find_control(id);
        if (!control)
            return -EINVAL;

        const auto &controls_map = camera->controls();
        auto it = controls_map.find(control);
        if (it == controls_map.end())
            return -ENOENT;

        switch (control->type()) {
        case ControlTypeBool:
            min = 0;
            max = 1;
            def = it->second.def().get<bool>();
            break;
        case ControlTypeInteger32:
            min = it->second.min().get<int32_t>();
            max = it->second.max().get<int32_t>();
            def = it->second.def().get<int32_t>();
            break;
        case ControlTypeInteger64:
            min = it->second.min().get<int64_t>();
            max = it->second.max().get<int64_t>();
            def = it->second.def().get<int64_t>();
            break;
        case ControlTypeFloat:
            min = std::lround(it->second.min().get<float>() * 1000.0f);
            max = std::lround(it->second.max().get<float>() * 1000.0f);
            def = std::lround(it->second.def().get<float>() * 1000.0f);
            break;
        default:
            return -EINVAL;
        }
        step = 1;
        name = control_to_name(control);
        return 0;
    }

    unsigned int numControls() const
    {
        unsigned int count = 0;
        for (const auto &entry : camera->controls())
            if (control_to_v4l(entry.first) && scalar_control(entry.first->type()))
                count++;
        return count;
    }

    int getControlInfo(unsigned int index, uint32_t &id, const char *&name,
                    int &type, int32_t &min, int32_t &max,
                    int32_t &def, int32_t &step) const
    {
        for (const auto &entry : camera->controls()) {
            if (!control_to_v4l(entry.first) || !scalar_control(entry.first->type()))
                continue;
            if (index--)
                continue;

            id = control_to_v4l(entry.first);

            if (controlInfo(id, min, max, def, step, name))
                return -EINVAL;

            switch (entry.first->type()) {
            case ControlTypeBool:
                type = LIBCAMERA_CONTROL_BOOL;
                break;
            case ControlTypeInteger64:
                type = LIBCAMERA_CONTROL_INTEGER64;
                break;
            case ControlTypeFloat:
                type = LIBCAMERA_CONTROL_FLOAT;
                break;
            default:
                type = LIBCAMERA_CONTROL_INTEGER;
                break;
            }
#ifdef CAMORAMA_LIBCAMERA_HAS_EXPOSURE_GAIN_MODE
            if (entry.first == &controls::ExposureTimeMode ||
                entry.first == &controls::AnalogueGainMode)
                type = LIBCAMERA_CONTROL_MENU;
#endif
            return 0;
        }
        return -ENOENT;
    }

    int setControl(uint32_t id, int32_t value)
    {
        int32_t min, max, def, step;
        const char *name;

        if (controlInfo(id, min, max, def, step, name))
            return -ENOENT;

        if (value < min || value > max)
            return -ERANGE;

        std::lock_guard<std::mutex> guard(control_mutex);

        control_values[id] = value;
        pending_control_values[id] = value;

        if (id == V4L2_CID_EXPOSURE_AUTO_PRIORITY) {
            const int32_t mode = value ? 0 : 1;
            control_values[V4L2_CID_EXPOSURE_AUTO] = mode;
            control_values[V4L2_CID_AUTOGAIN] = mode;
            pending_control_values[V4L2_CID_EXPOSURE_AUTO] = mode;
            pending_control_values[V4L2_CID_AUTOGAIN] = mode;
        }
        return 0;
    }

    int getControl(uint32_t id, int32_t &value) const
    {
        std::lock_guard<std::mutex> guard(control_mutex);
        auto it = control_values.find(id);

        if (it != control_values.end()) {
            value = it->second;
            return 0;
        }

        int32_t min, max, def, step;
        const char *name;

        if (controlInfo(id, min, max, def, step, name))
            return -ENOENT;
        value = def;
        return 0;
    }

    int setFrameDuration(uint64_t duration)
    {
        std::lock_guard<std::mutex> guard(control_mutex);
        frame_duration_us = duration;
        return 0;
    }

    uint64_t frameDuration() const
    {
        std::lock_guard<std::mutex> guard(control_mutex);
        return frame_duration_us;
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
    std::vector<FormatSupport> format_supports;
    Stream *stream = nullptr;
    unsigned int width = 0;
    unsigned int height = 0;
    unsigned int stride = 0;
    unsigned int frame_size = 0;
    bool configured = false;
    bool started = false;
    uint64_t frame_duration_us = 33333;
    std::map<uint32_t, int32_t> control_values;
    std::map<uint32_t, int32_t> pending_control_values;
    mutable std::mutex control_mutex;
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

/*
 * C-style handlers that are compatible with V4L2
 */

static int libcamera_get_size(const libcamera_bridge_t *bridge,
                              unsigned int index, unsigned int *width,
                              unsigned int *height,
                              unsigned int *pixformat)
{
    const libcamera_bridge::FormatSupport *format;

    if (index >= bridge->sizes.size())
        return -EINVAL;

    *width = bridge->sizes[index].width;
    *height = bridge->sizes[index].height;

    format = bridge->formatForSize(bridge->sizes[index]);
    if (!format)
        return -EINVAL;

    *pixformat = format->v4l2_pixel_format;
    return 0;
}

static void libcamera_try_set_win_info(cam_t *cam, unsigned int *width,
                                       unsigned int *height)
{
    const Size size = cam->libcamera->nearestSize(*width, *height);

    *width = size.width;
    *height = size.height;
}

static void libcamera_add_control(cam_t *cam, guint32 id, const char *name,
                                  int type, gint32 min, gint32 max,
                                  gint32 def, gint32 step)
{
    video_controls_t *control = g_new0(video_controls_t, 1);
    video_controls_t **tail = &cam->controls;

    while (*tail)
        tail = (video_controls_t **)&(*tail)->next;
    *tail = control;
    control->name = g_strdup(name);
    control->group = g_strdup(_("Image processing controls"));
    control->type = type == LIBCAMERA_CONTROL_BOOL ?
                    V4L2_CTRL_TYPE_BOOLEAN :
                    type == LIBCAMERA_CONTROL_MENU ?
                    V4L2_CTRL_TYPE_MENU : V4L2_CTRL_TYPE_INTEGER;
    control->id = id;
    control->min = min;
    control->max = max;
    control->def = def;
    control->step = step;
    control->cam = cam;

    if (type == LIBCAMERA_CONTROL_MENU) {
        control->menu_size = 2;
        control->menu = g_new0(video_control_menu_t, control->menu_size);
        control->menu[0].value = 0;
        control->menu[0].name = g_strdup(_("Auto"));
        control->menu[1].value = 1;
        control->menu[1].name = g_strdup(_("Manual"));
    }
}

static void show_libcamera_error(const char *operation, char *detail)
{
    char *message;

    message = g_strdup_printf(_("libcamera: %s: %s"), operation,
                              detail ? detail : _("unknown error"));
    error_dialog(message);
    g_free(message);
    free(detail);
}

static int libcamera_cam_open(cam_t *cam, int)
{
    std::unique_ptr<libcamera_bridge> bridge(new libcamera_bridge(cam->debug));
    const char *camera_id;

    bridge->open(cam->video_dev);

    cam->libcamera = bridge.release();
    camera_id = cam->libcamera->id.c_str();
    g_free(cam->video_dev);
    cam->video_dev = g_strdup(camera_id);

    return 0;
}

static int libcamera_cam_close(cam_t *cam)
{
    if (cam->libcamera) {
        delete cam->libcamera;
        cam->libcamera = NULL;
    }
    cam->dev = -1;

    return 0;
}

static unsigned char *libcamera_cam_read(cam_t *cam, unsigned char *output)
{
    size_t bytes_used = 0;
    int ret;

    ret = cam->libcamera->readFrame(cam->capture_input, cam->sizeimage, 1000,
                                    bytes_used);
    if (ret)
        return NULL;
    ret = img_convert_to_rgb24(cam, cam->capture_input, bytes_used, output);

    g_atomic_int_inc(&cam->frame_number);

    return output;
}

static void libcamera_get_supported_resolutions(cam_t *cam)
{
    libcamera_bridge_t *bridge = cam->libcamera;
    unsigned int count, i;

    free(cam->res);
    cam->res = NULL;
    cam->n_res = 0;

    count = bridge->sizes.size();
    if (!count)
        return;

    cam->res = (resolutions *)calloc(count, sizeof(*cam->res));
    if (!cam->res)
        return;

    for (i = 0; i < count; i++) {
        struct resolutions *res = &cam->res[cam->n_res];

        if (libcamera_get_size(bridge, i, &res->x, &res->y,
                                      &res->pixformat))
            continue;
        res->depth = 24;
        res->max_fps = -1;
        res->order = 0;
        cam->n_res++;
    }

    if (cam->debug) {
        for (i = 0; i < cam->n_res; i++) {
            unsigned int format = cam->res[i].pixformat;

            printf("  format: '%c%c%c%c', resolution: %ux%u\n",
                   format & 0xff, (format >> 8) & 0xff,
                   (format >> 16) & 0xff, (format >> 24) & 0xff,
                   cam->res[i].x, cam->res[i].y);
        }
    }
}

static int libcamera_camera_cap(cam_t *cam)
{
    const char *name;
    unsigned int i;

    cam->rdir_ok = FALSE;
    cam->min_width = (unsigned int)-1;
    cam->min_height = (unsigned int)-1;
    cam->max_width = 0;
    cam->max_height = 0;
    libcamera_get_supported_resolutions(cam);

    if (!cam->n_res) {
        show_libcamera_error(_("could not query camera formats"), NULL);
        return 1;
    }

    for (i = 0; i < cam->n_res; i++) {
        cam->min_width = MIN(cam->min_width, cam->res[i].x);
        cam->min_height = MIN(cam->min_height, cam->res[i].y);
        cam->max_width = MAX(cam->max_width, cam->res[i].x);
        cam->max_height = MAX(cam->max_height, cam->res[i].y);
    }

    if (!cam->width || !cam->height) {
        if (cam->size == PICMAX) {
            cam->width = cam->max_width;
            cam->height = cam->max_height;
        } else if (cam->size == PICMIN) {
            cam->width = cam->min_width;
            cam->height = cam->min_height;
        } else {
            cam->width = cam->max_width / 2;
            cam->height = cam->max_height / 2;
        }
    }
    libcamera_try_set_win_info(cam, &cam->width, &cam->height);

    name = cam->libcamera->name.c_str();
    g_strlcpy(cam->name, name ? name : cam->video_dev, sizeof(cam->name));

    cam->read = FALSE;
    cam->userptr = FALSE;

    return 0;
}

static void libcamera_set_win_info(cam_t *cam)
{
    unsigned int frame_size = 0;
    unsigned int pixformat = 0;
    unsigned int stride = 0;

    cam->libcamera->configure(cam->width, cam->height, stride, frame_size,
                              pixformat);

    cam->pixformat = pixformat;
    cam->bpp = 24;
    cam->bytesperline = stride;
    cam->sizeimage = frame_size;
}

static gboolean libcamera_set_frame_interval(cam_t *cam,
                                             const struct v4l2_fract *interval)
{
    uint64_t duration;

    if (!interval->numerator || !interval->denominator)
        return FALSE;
    duration = ((uint64_t)interval->numerator * 1000000 +
                interval->denominator / 2) / interval->denominator;

    if (!cam->libcamera->supports_frame_duration())
        return -ENOTSUP;

    return cam->libcamera->setFrameDuration(duration);
}

static gboolean libcamera_get_frame_interval(cam_t *cam,
                                             struct v4l2_fract *interval)
{
    uint64_t duration = cam->libcamera->frameDuration();

    if (!duration)
        return FALSE;
    interval->numerator = duration;
    interval->denominator = 1000000;

    return TRUE;
}

static gboolean libcamera_supports_frame_interval(cam_t *cam)
{
    return cam->libcamera->supports_frame_duration();
}

static GArray *libcamera_get_frame_intervals(cam_t *cam)
{
    static const struct v4l2_fract common[] = {
        { 1, 5 }, { 1, 10 }, { 1, 15 }, { 1, 24 },
        { 1, 30 }, { 1, 60 },
    };
    GArray *intervals = g_array_new(FALSE, FALSE,
                                    sizeof(struct v4l2_fract));
    struct v4l2_fract current = { };
    guint i;

    if (libcamera_get_frame_interval(cam, &current))
        g_array_append_val(intervals, current);
    if (libcamera_supports_frame_interval(cam))
        for (i = 0; i < G_N_ELEMENTS(common); i++)
            g_array_append_val(intervals, common[i]);
    if (!intervals->len) {
        current.numerator = 1;
        current.denominator = 30;
        g_array_append_val(intervals, current);
    }
    return intervals;
}

static void libcamera_get_win_info(cam_t *)
{
}

static void libcamera_get_pic_info(cam_t *cam)
{
    cam_query_controls(cam);
    cam->contrast = -1;
    cam->brightness = -1;
    cam->whiteness = -1;
    cam->colour = -1;
    cam->hue = -1;
    cam->zoom = -1;
    cam->zoom_cid = 0;
}

static void libcamera_start_streaming(cam_t *cam)
{
    cam->libcamera->startCapture();
}

static void libcamera_stop_streaming(cam_t *cam)
{
    if (cam->libcamera)
        cam->libcamera->stopCapture();
}

static void libcamera_print_cam(cam_t *cam)
{
    printf("\nCamera Info\n");
    printf("-----------\n");

    printf("backend = libcamera, device = %s, x = %u, y = %u\n",
           cam->video_dev, cam->width, cam->height);

    printf("format = %c%c%c%c, bits per pixel = %d\n",
           cam->pixformat & 0xff, (cam->pixformat >> 8) & 0xff,
           (cam->pixformat >> 16) & 0xff, (cam->pixformat >> 24) & 0xff,
           cam->bpp);
}

static void libcamera_cancel_read(cam_t *cam)
{
    libcamera_stop_streaming(cam);
}

static int libcamera_query_controls(cam_t *cam)
{
    unsigned int i, count;

    cam_free_controls(cam);
    count = cam->libcamera->numControls();
    for (i = 0; i < count; i++) {
        guint32 id;
        const char *name;
        int type;
        gint32 min, max, def, step;

        if (cam->libcamera->getControlInfo(i, id, name, type, min, max,
                                           def, step))
            continue;

        libcamera_add_control(cam, id, name, type, min, max, def, step);
    }
    if (cam->debug) {
        video_controls_t *control;

        printf("libcamera controls:\n");
        for (control = cam->controls; control; control = (video_controls_t *)control->next)
            printf("  %s: min=%d max=%d default=%d step=%d\n",
                   control->name, control->min, control->max,
                   control->def, control->step);
    }

    return 0;
}

static int libcamera_set_control(cam_t *cam, guint32 id, void *value)
{
    if (!cam_find_control_per_id(cam, id))
        return -1;

    return cam->libcamera->setControl(id, *(gint32 *)value);
}

static int libcamera_get_control(cam_t *cam, guint32 id, void *value)
{
    if (!cam_find_control_per_id(cam, id))
        return -1;

    return cam->libcamera->getControl(id, *(gint32 *)value);
}

static void libcamera_supported_resolutions(cam_t *cam,
                                             gboolean)
{
    libcamera_get_supported_resolutions(cam);
}

static void libcamera_try_win_info(cam_t *cam, unsigned int,
                                   unsigned int *width,
                                   unsigned int *height)
{
    libcamera_try_set_win_info(cam, width, height);
}

/*
 * Camorama C interface
 */

extern "C"
int libcamera_bridge_list_cameras(struct libcamera_camera_info **output,
                                  unsigned int *count)
{
    struct libcamera_camera_info *list;
    CameraManager manager;
    char *msg;
    int ret;

    if (!output || !count)
        return -EINVAL;

    *output = nullptr;
    *count = 0;

    ret = manager.start();
    if (ret) {
        msg = g_strdup_printf(_("failed to start camera manager: %d"), ret);
        error_dialog(msg);
        g_free(msg);
        return ret;
    }

    const auto available = manager.cameras();
    if (available.empty()) {
        manager.stop();
        error_dialog(_("no cameras were found"));
        return -ENODEV;
    }

    list = static_cast<struct libcamera_camera_info *>(
        calloc(available.size(), sizeof(*list)));
    if (!list) {
        manager.stop();
        error_dialog(_("failed to allocate camera list"));
        return -ENOMEM;
    }

    for (size_t i = 0; i < available.size(); i++) {
        const std::string id = available[i]->id();

        list[i].id = strdup(id.c_str());
        const std::string name = camera_model(available[i], id);

        list[i].name = strdup(name.c_str());
        if (!list[i].id || !list[i].name) {
            libcamera_bridge_free_cameras(list, available.size());
            manager.stop();
            error_dialog(_("failed to allocate camera information"));
            return -ENOMEM;
        }
    }

    *output = list;
    *count = available.size();
    manager.stop();

    return 0;
}

extern "C"
void libcamera_bridge_free_cameras(struct libcamera_camera_info *cameras,
                                   unsigned int count)
{
    if (!cameras)
        return;

    for (unsigned int i = 0; i < count; i++) {
        free(cameras[i].id);
        free(cameras[i].name);
    }

    free(cameras);
}

extern "C"
const struct camera_backend libcamera_camera_backend = {
    .name = "libcamera",
    .is_libcamera = TRUE,
    .open = libcamera_cam_open,
    .close = libcamera_cam_close,
    .read = libcamera_cam_read,
    .cancel_read = libcamera_cancel_read,
    .query_controls = libcamera_query_controls,
    .set_control = libcamera_set_control,
    .get_control = libcamera_get_control,
    .get_frame_intervals = libcamera_get_frame_intervals,
    .set_frame_interval = libcamera_set_frame_interval,
    .get_frame_interval = libcamera_get_frame_interval,
    .camera_cap = libcamera_camera_cap,
    .get_pic_info = libcamera_get_pic_info,
    .get_win_info = libcamera_get_win_info,
    .try_set_win_info = libcamera_try_win_info,
    .set_win_info = libcamera_set_win_info,
    .get_supported_resolutions = libcamera_supported_resolutions,
    .print_cam = libcamera_print_cam,
    .start_streaming = libcamera_start_streaming,
    .stop_streaming = libcamera_stop_streaming,
};
