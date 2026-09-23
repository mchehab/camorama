// SPDX-License-Identifier: GPL-2.0-or-later

#include <errno.h>

#include "mock_v4l.h"

const struct mock_v4l_size mock_c920_yuyv_sizes[] = {
    { 640, 480, 30 }, { 160, 90, 30 }, { 160, 120, 30 },
    { 176, 144, 30 }, { 320, 180, 30 }, { 320, 240, 30 },
    { 352, 288, 30 }, { 432, 240, 30 }, { 640, 360, 30 },
    { 800, 448, 30 }, { 800, 600, 24 }, { 864, 480, 24 },
    { 960, 720, 15 }, { 1024, 576, 15 }, { 1280, 720, 10 },
    { 1600, 896, 7.5 }, { 1920, 1080, 5 }, { 2304, 1296, 2 },
    { 2304, 1536, 2 },
};
const unsigned int mock_c920_yuyv_sizes_count =
    sizeof(mock_c920_yuyv_sizes) / sizeof(mock_c920_yuyv_sizes[0]);

const struct mock_v4l_size mock_c920_compressed_sizes[] = {
    { 640, 480, 30 }, { 160, 90, 30 }, { 160, 120, 30 },
    { 176, 144, 30 }, { 320, 180, 30 }, { 320, 240, 30 },
    { 352, 288, 30 }, { 432, 240, 30 }, { 640, 360, 30 },
    { 800, 448, 30 }, { 800, 600, 30 }, { 864, 480, 30 },
    { 960, 720, 30 }, { 1024, 576, 30 }, { 1280, 720, 30 },
    { 1600, 896, 30 }, { 1920, 1080, 30 },
};
const unsigned int mock_c920_compressed_sizes_count =
    sizeof(mock_c920_compressed_sizes) / sizeof(mock_c920_compressed_sizes[0]);

static unsigned int c920_format(unsigned int index)
{
    static const unsigned int formats[] = {
        V4L2_PIX_FMT_YUYV, V4L2_PIX_FMT_H264, V4L2_PIX_FMT_MJPEG,
    };
    return formats[index];
}

static const struct mock_v4l_size *c920_sizes(unsigned int pixformat,
                                              unsigned int *count)
{
    if (pixformat == V4L2_PIX_FMT_YUYV) {
        *count = mock_c920_yuyv_sizes_count;
        return mock_c920_yuyv_sizes;
    }
    *count = mock_c920_compressed_sizes_count;
    return mock_c920_compressed_sizes;
}

static int c920_ioctl(int fd, unsigned long request, void *arg)
{
    (void)fd;
    if (request == VIDIOC_ENUM_FMT) {
        struct v4l2_fmtdesc *fmt = arg;
        if (fmt->index >= 3)
            goto invalid;
        fmt->pixelformat = c920_format(fmt->index);
        fmt->flags = fmt->index ? V4L2_FMT_FLAG_COMPRESSED : 0;
        return 0;
    }
    if (request == VIDIOC_TRY_FMT)
        return 0;
    if (request == VIDIOC_ENUM_FRAMESIZES) {
        struct v4l2_frmsizeenum *size = arg;
        unsigned int count;
        const struct mock_v4l_size *sizes = c920_sizes(size->pixel_format,
                                                       &count);
        if (size->index >= count)
            goto invalid;
        size->type = V4L2_FRMSIZE_TYPE_DISCRETE;
        size->discrete.width = sizes[size->index].width;
        size->discrete.height = sizes[size->index].height;
        return 0;
    }
    if (request == VIDIOC_ENUM_FRAMEINTERVALS) {
        struct v4l2_frmivalenum *iv = arg;
        unsigned int count, n_rates, i;
        const struct mock_v4l_size *sizes = c920_sizes(iv->pixel_format,
                                                       &count);
        const struct mock_v4l_size *size = NULL;
        static const float common[] = { 30, 24, 20, 15, 10, 7.5, 5 };
        static const float reduced[] = { 24, 20, 15, 10, 7.5, 5 };
        static const float low[] = { 15, 10, 7.5, 5 };
        static const float hd[] = { 10, 7.5, 5 };
        static const float large[] = { 7.5, 5 };
        static const float slow[] = { 5 };
        static const float very_slow[] = { 2 };
        const float *rates = common;

        for (i = 0; i < count; i++)
            if (sizes[i].width == iv->width && sizes[i].height == iv->height) {
                size = &sizes[i];
                break;
            }
        if (!size)
            goto invalid;

        if (iv->pixel_format == V4L2_PIX_FMT_YUYV) {
            n_rates = sizeof(common) / sizeof(common[0]);
            if ((size->width == 800 && size->height == 600) ||
                size->width == 864) {
                rates = reduced;
                n_rates = sizeof(reduced) / sizeof(reduced[0]);
            } else if (size->width == 960 || size->width == 1024) {
                rates = low;
                n_rates = sizeof(low) / sizeof(low[0]);
            } else if (size->width == 1280) {
                rates = hd;
                n_rates = sizeof(hd) / sizeof(hd[0]);
            } else if (size->width == 1600) {
                rates = large;
                n_rates = sizeof(large) / sizeof(large[0]);
            } else if (size->width >= 1920) {
                rates = size->width == 2304 ? very_slow : slow;
                n_rates = size->width == 2304 ? 1 : 1;
            }
        } else {
            n_rates = sizeof(common) / sizeof(common[0]);
        }
        if (iv->index >= n_rates)
            goto invalid;

        iv->type = V4L2_FRMIVAL_TYPE_DISCRETE;
        if (rates[iv->index] == 7.5) {
            iv->discrete.numerator = 2;
            iv->discrete.denominator = 15;
        } else {
            iv->discrete.numerator = 1;
            iv->discrete.denominator = rates[iv->index];
        }
        return 0;
    }
invalid:
    errno = EINVAL;
    return -1;
}

static int c920_open(const char *path, int flags)
{
    (void)path;
    (void)flags;
    return 73; /* Fake descriptor; never passed to the operating system. */
}

static int c920_close(int fd)
{
    return fd == 73 ? 0 : -1;
}

static int c920_read(cam_t *cam, void *buffer, size_t size)
{
    static unsigned int frame;
    unsigned char *yuyv = buffer;
    size_t row, pair;
    size_t stride = (size_t)cam->width * 2;

    if (!cam->width || !cam->height || (cam->width & 1) ||
        size < stride * cam->height) {
        errno = EINVAL;
        return -1;
    }

    for (row = 0; row < cam->height; row++) {
        for (pair = 0; pair < cam->width / 2; pair++) {
            unsigned int x = (pair * 2 + frame * 7) % cam->width;
            unsigned char y0 = x * 255 / cam->width;
            unsigned char y1 = ((x + 1) % cam->width) * 255 / cam->width;
            size_t offset = row * stride + pair * 4;
            yuyv[offset] = y0;
            yuyv[offset + 1] = 128;
            yuyv[offset + 2] = y1;
            yuyv[offset + 3] = 128;
        }
    }
    frame++;
    return 0;
}

const struct cam_v4l_ops mock_c920_v4l_ops = {
    .open = c920_open,
    .close = c920_close,
    .read = c920_read,
    .ioctl = c920_ioctl,
};
