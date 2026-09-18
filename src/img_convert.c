#include <config.h>

#include <errno.h>
#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "img_convert.h"
#include "v4l.h"

#ifdef HAVE_FFMPEG
#include <libavcodec/avcodec.h>
#include <libavutil/error.h>
#include <libavutil/pixfmt.h>
#include <libswscale/swscale.h>
#endif

#define BYTE_CLAMP(a) CLAMP(a, 0, 255)

struct img_converter {
#ifdef HAVE_FFMPEG
    AVCodecContext *codec;
    AVFrame *frame;
    AVPacket *packet;
    struct SwsContext *sws;
    unsigned int pixformat;
#else
    int unavailable;
#endif
};

/* Formats that are natively supported */
static const struct img_format supported_formats[] = {
    { V4L2_PIX_FMT_RGB24,   24, -1, -1, 1},
    { V4L2_PIX_FMT_BGR24,   24, -1, -1, 1},

    { V4L2_PIX_FMT_YUYV,    16, -1, -1, -1},
    { V4L2_PIX_FMT_UYVY,    16, -1, -1, -1},
    { V4L2_PIX_FMT_YVYU,    16, -1, -1, -1},
    { V4L2_PIX_FMT_VYUY,    16, -1, -1, -1},
    { V4L2_PIX_FMT_NV12,     8, 1, -1, -1},
    { V4L2_PIX_FMT_NV21,     8, 1, -1, -1},
    { V4L2_PIX_FMT_NV16,     8, 0, -1, -1},
    { V4L2_PIX_FMT_NV61,     8, 0, -1, -1},
    { V4L2_PIX_FMT_YUV420,   8, 1, 1, -1},
    { V4L2_PIX_FMT_YVU420,   8, 1, 1, -1},
    { V4L2_PIX_FMT_YUV422P,  8, 0, 1, 0},

    { V4L2_PIX_FMT_RGB565,  16, -1, -1, 1},
    { V4L2_PIX_FMT_RGB565X, 16, -1, -1, 1},

    { V4L2_PIX_FMT_BGR32,   32, -1, -1, 1},
    { V4L2_PIX_FMT_ABGR32,  32, -1, -1, 1},
    { V4L2_PIX_FMT_XBGR32,  32, -1, -1, 1},
    { V4L2_PIX_FMT_RGB32,   32, -1, -1, 1},
    { V4L2_PIX_FMT_ARGB32,  32, -1, -1, 1},
    { V4L2_PIX_FMT_XRGB32,  32, -1, -1, 1},

    { V4L2_PIX_FMT_SBGGR8,   8, -1, -1, 1},
    { V4L2_PIX_FMT_SGBRG8,   8, -1, -1, 1},
    { V4L2_PIX_FMT_SGRBG8,   8, -1, -1, 1},
    { V4L2_PIX_FMT_SRGGB8,   8, -1, -1, 1},
#ifdef HAVE_FFMPEG
    { V4L2_PIX_FMT_MJPEG,     0, -1, -1, 1},
    { V4L2_PIX_FMT_H264,      0, -1, -1, 1},
#endif
};

#define ARRAY_SIZE(a)  (sizeof(a)/sizeof(*a))

/*
 * Bayer conversion imported from libv4lconvert in v4l-utils.
 *
 * Copyright 2008 Hans de Goede <hdegoede@redhat.com>
 *
 * Thanks also to Damien Douxchamps and Frederic Devernay, who wrote the
 * original libdc1394 implementation on which this code is based, and to
 * the OpenCV authors whose Bayer decoder inspired it.
 *
 * The imported code was published under LGPL-2.1-or-later. It is
 * distributed here under Camorama's GPL-2.0-or-later license.
 */

/**************************************************************
 *     Color conversion functions for cameras that can        *
 * output raw-Bayer pattern images, such as some Basler and   *
 * Point Grey camera. Most of the algos presented here come   *
 * from http://www-ise.stanford.edu/~tingchen/ and have been  *
 * converted from Matlab to C and extended to all elementary  *
 * patterns.                                                  *
 **************************************************************/

/* inspired by OpenCV's Bayer decoding */
static void border_bayer_line_to_rgb24(
		const unsigned char *bayer, const unsigned char *adjacent_bayer,
		unsigned char *bgr, int width, const int start_with_green, const int blue_line)
{
	int t0, t1;

	if (start_with_green) {
		/* First pixel */
		if (blue_line) {
			*bgr++ = bayer[1];
			*bgr++ = bayer[0];
			*bgr++ = adjacent_bayer[0];
		} else {
			*bgr++ = adjacent_bayer[0];
			*bgr++ = bayer[0];
			*bgr++ = bayer[1];
		}
		/* Second pixel */
		t0 = (bayer[0] + bayer[2] + adjacent_bayer[1] + 1) / 3;
		t1 = (adjacent_bayer[0] + adjacent_bayer[2] + 1) >> 1;
		if (blue_line) {
			*bgr++ = bayer[1];
			*bgr++ = t0;
			*bgr++ = t1;
		} else {
			*bgr++ = t1;
			*bgr++ = t0;
			*bgr++ = bayer[1];
		}
		bayer++;
		adjacent_bayer++;
		width -= 2;
	} else {
		/* First pixel */
		t0 = (bayer[1] + adjacent_bayer[0] + 1) >> 1;
		if (blue_line) {
			*bgr++ = bayer[0];
			*bgr++ = t0;
			*bgr++ = adjacent_bayer[1];
		} else {
			*bgr++ = adjacent_bayer[1];
			*bgr++ = t0;
			*bgr++ = bayer[0];
		}
		width--;
	}

	if (blue_line) {
		for ( ; width > 2; width -= 2) {
			t0 = (bayer[0] + bayer[2] + 1) >> 1;
			*bgr++ = t0;
			*bgr++ = bayer[1];
			*bgr++ = adjacent_bayer[1];
			bayer++;
			adjacent_bayer++;

			t0 = (bayer[0] + bayer[2] + adjacent_bayer[1] + 1) / 3;
			t1 = (adjacent_bayer[0] + adjacent_bayer[2] + 1) >> 1;
			*bgr++ = bayer[1];
			*bgr++ = t0;
			*bgr++ = t1;
			bayer++;
			adjacent_bayer++;
		}
	} else {
		for ( ; width > 2; width -= 2) {
			t0 = (bayer[0] + bayer[2] + 1) >> 1;
			*bgr++ = adjacent_bayer[1];
			*bgr++ = bayer[1];
			*bgr++ = t0;
			bayer++;
			adjacent_bayer++;

			t0 = (bayer[0] + bayer[2] + adjacent_bayer[1] + 1) / 3;
			t1 = (adjacent_bayer[0] + adjacent_bayer[2] + 1) >> 1;
			*bgr++ = t1;
			*bgr++ = t0;
			*bgr++ = bayer[1];
			bayer++;
			adjacent_bayer++;
		}
	}

	if (width == 2) {
		/* Second to last pixel */
		t0 = (bayer[0] + bayer[2] + 1) >> 1;
		if (blue_line) {
			*bgr++ = t0;
			*bgr++ = bayer[1];
			*bgr++ = adjacent_bayer[1];
		} else {
			*bgr++ = adjacent_bayer[1];
			*bgr++ = bayer[1];
			*bgr++ = t0;
		}
		/* Last pixel */
		t0 = (bayer[1] + adjacent_bayer[2] + 1) >> 1;
		if (blue_line) {
			*bgr++ = bayer[2];
			*bgr++ = t0;
			*bgr++ = adjacent_bayer[1];
		} else {
			*bgr++ = adjacent_bayer[1];
			*bgr++ = t0;
			*bgr++ = bayer[2];
		}
	} else {
		/* Last pixel */
		if (blue_line) {
			*bgr++ = bayer[0];
			*bgr++ = bayer[1];
			*bgr++ = adjacent_bayer[1];
		} else {
			*bgr++ = adjacent_bayer[1];
			*bgr++ = bayer[1];
			*bgr++ = bayer[0];
		}
	}
}

/* From libdc1394, which on turn was based on OpenCV's Bayer decoding */
static void bayer_to_rgb24(const unsigned char *bayer,
		unsigned char *bgr, int width, int height, const unsigned int stride, int start_with_green,
		int blue_line)
{
	/* render the first line */
	border_bayer_line_to_rgb24(bayer, bayer + stride, bgr, width,
			start_with_green, blue_line);
	bgr += width * 3;

	/* reduce height by 2 because of the special case top/bottom line */
	for (height -= 2; height; height--) {
		int t0, t1;
		/* (width - 2) because of the border */
		const unsigned char *bayer_end = bayer + (width - 2);

		if (start_with_green) {

			t0 = (bayer[1] + bayer[stride * 2 + 1] + 1) >> 1;
			/* Write first pixel */
			t1 = (bayer[0] + bayer[stride * 2] + bayer[stride + 1] + 1) / 3;
			if (blue_line) {
				*bgr++ = t0;
				*bgr++ = t1;
				*bgr++ = bayer[stride];
			} else {
				*bgr++ = bayer[stride];
				*bgr++ = t1;
				*bgr++ = t0;
			}

			/* Write second pixel */
			t1 = (bayer[stride] + bayer[stride + 2] + 1) >> 1;
			if (blue_line) {
				*bgr++ = t0;
				*bgr++ = bayer[stride + 1];
				*bgr++ = t1;
			} else {
				*bgr++ = t1;
				*bgr++ = bayer[stride + 1];
				*bgr++ = t0;
			}
			bayer++;
		} else {
			/* Write first pixel */
			t0 = (bayer[0] + bayer[stride * 2] + 1) >> 1;
			if (blue_line) {
				*bgr++ = t0;
				*bgr++ = bayer[stride];
				*bgr++ = bayer[stride + 1];
			} else {
				*bgr++ = bayer[stride + 1];
				*bgr++ = bayer[stride];
				*bgr++ = t0;
			}
		}

		if (blue_line) {
			for (; bayer <= bayer_end - 2; bayer += 2) {
				t0 = (bayer[0] + bayer[2] + bayer[stride * 2] +
					bayer[stride * 2 + 2] + 2) >> 2;
				t1 = (bayer[1] + bayer[stride] + bayer[stride + 2] +
					bayer[stride * 2 + 1] + 2) >> 2;
				*bgr++ = t0;
				*bgr++ = t1;
				*bgr++ = bayer[stride + 1];

				t0 = (bayer[2] + bayer[stride * 2 + 2] + 1) >> 1;
				t1 = (bayer[stride + 1] + bayer[stride + 3] + 1) >> 1;
				*bgr++ = t0;
				*bgr++ = bayer[stride + 2];
				*bgr++ = t1;
			}
		} else {
			for (; bayer <= bayer_end - 2; bayer += 2) {
				t0 = (bayer[0] + bayer[2] + bayer[stride * 2] +
					bayer[stride * 2 + 2] + 2) >> 2;
				t1 = (bayer[1] + bayer[stride] + bayer[stride + 2] +
					bayer[stride * 2 + 1] + 2) >> 2;
				*bgr++ = bayer[stride + 1];
				*bgr++ = t1;
				*bgr++ = t0;

				t0 = (bayer[2] + bayer[stride * 2 + 2] + 1) >> 1;
				t1 = (bayer[stride + 1] + bayer[stride + 3] + 1) >> 1;
				*bgr++ = t1;
				*bgr++ = bayer[stride + 2];
				*bgr++ = t0;
			}
		}

		if (bayer < bayer_end) {
			/* write second to last pixel */
			t0 = (bayer[0] + bayer[2] + bayer[stride * 2] +
				bayer[stride * 2 + 2] + 2) >> 2;
			t1 = (bayer[1] + bayer[stride] + bayer[stride + 2] +
				bayer[stride * 2 + 1] + 2) >> 2;
			if (blue_line) {
				*bgr++ = t0;
				*bgr++ = t1;
				*bgr++ = bayer[stride + 1];
			} else {
				*bgr++ = bayer[stride + 1];
				*bgr++ = t1;
				*bgr++ = t0;
			}
			/* write last pixel */
			t0 = (bayer[2] + bayer[stride * 2 + 2] + 1) >> 1;
			if (blue_line) {
				*bgr++ = t0;
				*bgr++ = bayer[stride + 2];
				*bgr++ = bayer[stride + 1];
			} else {
				*bgr++ = bayer[stride + 1];
				*bgr++ = bayer[stride + 2];
				*bgr++ = t0;
			}

			bayer++;

		} else {
			/* write last pixel */
			t0 = (bayer[0] + bayer[stride * 2] + 1) >> 1;
			t1 = (bayer[1] + bayer[stride * 2 + 1] + bayer[stride] + 1) / 3;
			if (blue_line) {
				*bgr++ = t0;
				*bgr++ = t1;
				*bgr++ = bayer[stride + 1];
			} else {
				*bgr++ = bayer[stride + 1];
				*bgr++ = t1;
				*bgr++ = t0;
			}

		}

		/* skip 2 border pixels and padding */
		bayer += (stride - width) + 2;

		blue_line = !blue_line;
		start_with_green = !start_with_green;
	}

	/* render the last line */
	border_bayer_line_to_rgb24(bayer + stride, bayer, bgr, width,
			!start_with_green, !blue_line);
}

static void convert_bayer_to_rgb24(const unsigned char *bayer,
		unsigned char *bgr, int width, int height, const unsigned int stride, unsigned int pixfmt)
{
	bayer_to_rgb24(bayer, bgr, width, height, stride,
			pixfmt == V4L2_PIX_FMT_SGBRG8		/* start with green */
			|| pixfmt == V4L2_PIX_FMT_SGRBG8,
			pixfmt != V4L2_PIX_FMT_SBGGR8		/* blue line */
			&& pixfmt != V4L2_PIX_FMT_SGBRG8);
}
const struct img_format *img_format_get(unsigned int pixformat)
{
    unsigned int i;

    for (i = 0; i < ARRAY_SIZE(supported_formats); i++)
        if (supported_formats[i].pixformat == pixformat)
            return &supported_formats[i];

    return NULL;
}

unsigned int img_format_order(unsigned int pixformat)
{
    const struct img_format *format = img_format_get(pixformat);

    if (format)
        return format - supported_formats;
    return ARRAY_SIZE(supported_formats);
}

static void convert_yuv(struct colorspace_parms *c,
                        int32_t y, int32_t u, int32_t v,
                        unsigned char **dst)
{
        if (c->quantization == V4L2_QUANTIZATION_FULL_RANGE)
                y *= 65536;
        else
                y = (y - 16) * 76284;

    u -= 128;
    v -= 128;

    /*
     * TODO: add BT2020 and SMPTE240M and better handle
     * other differences
     */
    switch (c->ycbcr_enc) {
    case V4L2_YCBCR_ENC_601:
    case V4L2_YCBCR_ENC_XV601:
    case V4L2_YCBCR_ENC_SYCC:
        /*
         * ITU-R BT.601 matrix:
         *    R = 1.164 * y +    0.0 * u +  1.596 * v
         *    G = 1.164 * y + -0.392 * u + -0.813 * v
         *    B = 1.164 * y +  2.017 * u +    0.0 * v
         */
        *(*dst)++ = BYTE_CLAMP((y              + 104595 * v) >> 16);
        *(*dst)++ = BYTE_CLAMP((y -  25690 * u -  53281 * v) >> 16);
        *(*dst)++ = BYTE_CLAMP((y + 132186 * u             ) >> 16);
        break;
    case V4L2_YCBCR_ENC_DEFAULT:
    case V4L2_YCBCR_ENC_709:
    case V4L2_YCBCR_ENC_XV709:
    case V4L2_YCBCR_ENC_BT2020:
    case V4L2_YCBCR_ENC_BT2020_CONST_LUM:
    case V4L2_YCBCR_ENC_SMPTE240M:
    default:
        /*
         * ITU-R BT.709 matrix:
         *    R = 1.164 * y +    0.0 * u +  1.793 * v
         *    G = 1.164 * y + -0.213 * u + -0.533 * v
         *    B = 1.164 * y +  2.112 * u +    0.0 * v
         */
        *(*dst)++ = BYTE_CLAMP((y              + 117506 * v) >> 16);
        *(*dst)++ = BYTE_CLAMP((y -  13959 * u -  34931 * v) >> 16);
        *(*dst)++ = BYTE_CLAMP((y + 138412 * u             ) >> 16);
        break;
    }
}

static void copy_two_pixels(cam_t *cam,
                            struct colorspace_parms *c,
                            unsigned char *plane0,
                            unsigned char *plane1,
                            unsigned char *plane2,
                            unsigned char **dst)
{
    uint32_t fourcc = cam->pixformat;
    int32_t y_off, u_off, u, v;
    uint16_t pix;
    int i;

    switch (cam->pixformat) {
    case V4L2_PIX_FMT_RGB565: /* rrrrrggg gggbbbbb */
        for (i = 0; i < 2; i++) {
            pix = (plane0[0] << 8) + plane0[1];

            *(*dst)++ = (unsigned char)(((pix & 0xf800) >> 11) << 3) | 0x07;
            *(*dst)++ = (unsigned char)((((pix & 0x07e0) >> 5)) << 2) | 0x03;
            *(*dst)++ = (unsigned char)((pix & 0x1f) << 3) | 0x07;

            plane0 += 2;
        }
        break;
    case V4L2_PIX_FMT_RGB565X: /* gggbbbbb rrrrrggg */
        for (i = 0; i < 2; i++) {
            pix = (plane0[1] << 8) + plane0[0];

            *(*dst)++ = (unsigned char)(((pix & 0xf800) >> 11) << 3) | 0x07;
            *(*dst)++ = (unsigned char)((((pix & 0x07e0) >> 5)) << 2) | 0x03;
            *(*dst)++ = (unsigned char)((pix & 0x1f) << 3) | 0x07;

            plane0 += 2;
        }
        break;
    case V4L2_PIX_FMT_YUYV:
    case V4L2_PIX_FMT_UYVY:
    case V4L2_PIX_FMT_YVYU:
    case V4L2_PIX_FMT_VYUY:
        y_off = (fourcc == V4L2_PIX_FMT_YUYV || fourcc == V4L2_PIX_FMT_YVYU) ? 0 : 1;
        u_off = (fourcc == V4L2_PIX_FMT_YUYV || fourcc == V4L2_PIX_FMT_UYVY) ? 0 : 2;

        u = plane0[(1 - y_off) + u_off];
        v = plane0[(1 - y_off) + (2 - u_off)];

        for (i = 0; i < 2; i++)
            convert_yuv(c, plane0[y_off + (i << 1)], u, v, dst);

        break;
    case V4L2_PIX_FMT_NV12:
    case V4L2_PIX_FMT_NV16:
        u = plane1[0];
        v = plane1[1];

        for (i = 0; i < 2; i++)
            convert_yuv(c, plane0[i], u, v, dst);

        break;
    case V4L2_PIX_FMT_NV21:
    case V4L2_PIX_FMT_NV61:
        v = plane1[0];
        u = plane1[1];

        for (i = 0; i < 2; i++)
            convert_yuv(c, plane0[i], u, v, dst);

        break;
    case V4L2_PIX_FMT_YUV420:
    case V4L2_PIX_FMT_YUV422P:
        u = plane1[0];
        v = plane2[0];

        for (i = 0; i < 2; i++)
            convert_yuv(c, plane0[i], u, v, dst);

        break;
    case V4L2_PIX_FMT_YVU420:
        v = plane1[0];
        u = plane2[0];

        for (i = 0; i < 2; i++)
            convert_yuv(c, plane0[i], u, v, dst);

        break;
    case V4L2_PIX_FMT_RGB32:
    case V4L2_PIX_FMT_ARGB32:
    case V4L2_PIX_FMT_XRGB32:
        for (i = 0; i < 2; i++) {
            *(*dst)++ = plane0[1];
            *(*dst)++ = plane0[2];
            *(*dst)++ = plane0[3];

            plane0 += 4;
        }
        break;
    case V4L2_PIX_FMT_BGR32:
    case V4L2_PIX_FMT_ABGR32:
    case V4L2_PIX_FMT_XBGR32:
        for (i = 0; i < 2; i++) {
            *(*dst)++ = plane0[2];
            *(*dst)++ = plane0[1];
            *(*dst)++ = plane0[0];

            plane0 += 4;
        }
        break;
    default:
    case V4L2_PIX_FMT_BGR24:
        for (i = 0; i < 2; i++) {
            *(*dst)++ = plane0[2];
            *(*dst)++ = plane0[1];
            *(*dst)++ = plane0[0];

            plane0 += 3;
        }
        break;
    }
}

int img_convert_to_rgb24(cam_t *cam, unsigned char *inbuf,
                         size_t input_size)
{
    unsigned char *plane0 = inbuf;
    unsigned char *p_out = cam->pic_buf;
    uint32_t width = cam->width;
    uint32_t height = cam->height;
    uint32_t bytesperline = cam->bytesperline;
    const struct img_format *video_fmt;
    unsigned char *plane0_start = plane0;
    unsigned char *plane1_start = NULL;
    unsigned char *plane2_start = NULL;
    unsigned char *plane1 = NULL;
    unsigned char *plane2 = NULL;
    unsigned int x, y, depth;
    uint32_t num_planes = 1;
    unsigned char *p_start;
    uint32_t plane0_size;
    uint32_t w_dec = 0;
    uint32_t h_dec = 0;

    video_fmt = img_format_get(cam->pixformat);
    if (!video_fmt)
        return -ENOTSUP;

    if (img_codec_supported(cam->pixformat))
        return img_decode_to_rgb24(&cam->converter, cam->pixformat,
                                   inbuf, input_size, cam->pic_buf,
                                   width, height);

    switch (cam->pixformat) {
    case V4L2_PIX_FMT_RGB24:
        for (y = 0; y < height; y++)
            memcpy(cam->pic_buf + y * width * 3,
                   inbuf + y * bytesperline, width * 3);
        return width * height * 3;
    case V4L2_PIX_FMT_SBGGR8:
    case V4L2_PIX_FMT_SGBRG8:
    case V4L2_PIX_FMT_SGRBG8:
    case V4L2_PIX_FMT_SRGGB8:
        if (width < 3 || height < 2 || bytesperline < width)
            return 0;
        convert_bayer_to_rgb24(inbuf, cam->pic_buf, width, height,
                               bytesperline, cam->pixformat);
        return width * height * 3;
    default:
        break;
    }

    depth = video_fmt->depth;

    if (video_fmt->y_decimation >= 0) {
        num_planes++;
        h_dec = video_fmt->y_decimation;
    }

    if (video_fmt->x_decimation >= 0) {
        num_planes++;
        w_dec = video_fmt->x_decimation;
    }

    p_start = p_out;

    if (num_planes > 1) {
        plane0_size = (width * height * depth) >> 3;
        plane1_start = plane0_start + plane0_size;
    }

    if (num_planes > 2)
        plane2_start = plane1_start + (plane0_size >> (w_dec + h_dec));

    for (y = 0; y < height; y++) {
        plane0 = plane0_start + bytesperline * y;
        if (num_planes > 1)
            plane1 = plane1_start + (bytesperline >> w_dec) * (y >> h_dec);
        if (num_planes > 2)
            plane2 = plane2_start + (bytesperline >> w_dec) * (y >> h_dec);

        for (x = 0; x < width >> 1; x++) {
            copy_two_pixels(cam, &cam->colorspc, plane0, plane1, plane2, &p_out);

            plane0 += depth >> 2;
            if (num_planes > 1)
                plane1 += depth >> (2 + w_dec);
            if (num_planes > 2)
                plane2 += depth >> (2 + w_dec);
        }
    }

    return p_out - p_start;
}

void img_get_colorspace_data(cam_t *cam,
                                struct v4l2_format *fmt)
{
    struct colorspace_parms *c = &cam->colorspc;
    const struct img_format *video_fmt;

    memset(c, 0, sizeof(*c));

    video_fmt = img_format_get(fmt->fmt.pix.pixelformat);
    if (!video_fmt)
            return;

    /*
     * A more complete colorspace default detection would need to
     * implement timings API, in order to check for SDTV/HDTV.
     */
    if (fmt->fmt.pix.colorspace == V4L2_COLORSPACE_DEFAULT)
        c->colorspace = video_fmt->is_rgb ?
                        V4L2_COLORSPACE_SRGB :
                        V4L2_COLORSPACE_REC709;
    else
        c->colorspace = fmt->fmt.pix.colorspace;

    if (fmt->fmt.pix.xfer_func == V4L2_XFER_FUNC_DEFAULT)
        c->xfer_func = V4L2_MAP_XFER_FUNC_DEFAULT(c->colorspace);
    else
        c->xfer_func = fmt->fmt.pix.xfer_func;

    if (!video_fmt->is_rgb) {
        if (fmt->fmt.pix.ycbcr_enc == V4L2_YCBCR_ENC_DEFAULT)
            c->ycbcr_enc = V4L2_MAP_YCBCR_ENC_DEFAULT(c->colorspace);
        else
            c->ycbcr_enc = fmt->fmt.pix.ycbcr_enc;
    }

    if (fmt->fmt.pix.quantization == V4L2_QUANTIZATION_DEFAULT)
        c->quantization = V4L2_MAP_QUANTIZATION_DEFAULT(video_fmt->is_rgb,
                                                        c->colorspace,
                                                        c->ycbcr_enc);

    if (cam->debug == TRUE) {
        if (!video_fmt->is_rgb) {
            printf("YUV standard: ");
                switch (c->ycbcr_enc) {
                case V4L2_YCBCR_ENC_601:
                case V4L2_YCBCR_ENC_XV601:
                case V4L2_YCBCR_ENC_SYCC:
                    printf("BT.601\n");
                    break;
                case V4L2_YCBCR_ENC_DEFAULT:
                case V4L2_YCBCR_ENC_709:
                case V4L2_YCBCR_ENC_XV709:
                case V4L2_YCBCR_ENC_BT2020:
                case V4L2_YCBCR_ENC_BT2020_CONST_LUM:
                case V4L2_YCBCR_ENC_SMPTE240M:
                default:
                    printf("BT.709\n");
                }
            }

        printf ("Quantization: %s\n",
                (c->quantization == V4L2_QUANTIZATION_FULL_RANGE) ?
                "full-range" : "limited-range");
    }
}

gboolean img_codec_supported(unsigned int pixformat)
{
#ifdef HAVE_FFMPEG
    return pixformat == V4L2_PIX_FMT_MJPEG ||
           pixformat == V4L2_PIX_FMT_H264;
#else
    (void)pixformat;
    return FALSE;
#endif
}

#ifdef HAVE_FFMPEG
static enum AVCodecID codec_id_from_pixformat(unsigned int pixformat)
{
    switch (pixformat) {
    case V4L2_PIX_FMT_MJPEG:
        return AV_CODEC_ID_MJPEG;
    case V4L2_PIX_FMT_H264:
        return AV_CODEC_ID_H264;
    default:
        return AV_CODEC_ID_NONE;
    }
}

static int prepare_converter(img_converter_t **converter,
                             unsigned int pixformat)
{
    img_converter_t *new_converter;
    const AVCodec *codec;
    enum AVCodecID codec_id;
    int ret;

    if (*converter && (*converter)->pixformat == pixformat)
        return 0;

    img_converter_free(*converter);
    *converter = NULL;

    codec_id = codec_id_from_pixformat(pixformat);
    codec = avcodec_find_decoder(codec_id);
    if (!codec)
        return AVERROR_DECODER_NOT_FOUND;

    new_converter = g_new0(img_converter_t, 1);
    new_converter->codec = avcodec_alloc_context3(codec);
    new_converter->frame = av_frame_alloc();
    new_converter->packet = av_packet_alloc();
    if (!new_converter->codec || !new_converter->frame ||
        !new_converter->packet) {
        img_converter_free(new_converter);
        return AVERROR(ENOMEM);
    }

    ret = avcodec_open2(new_converter->codec, codec, NULL);
    if (ret < 0) {
        img_converter_free(new_converter);
        return ret;
    }

    new_converter->pixformat = pixformat;
    *converter = new_converter;
    return 0;
}
#endif

int img_decode_to_rgb24(img_converter_t **converter,
                        unsigned int pixformat,
                        const unsigned char *input, size_t input_size,
                        unsigned char *output,
                        unsigned int width, unsigned int height)
{
#ifdef HAVE_FFMPEG
    uint8_t *destination[] = { output, NULL, NULL, NULL };
    int destination_stride[] = { width * 3, 0, 0, 0 };
    img_converter_t *state;
    int ret;

    if (!converter || !input || !output || input_size > INT_MAX)
        return AVERROR(EINVAL);

    ret = prepare_converter(converter, pixformat);
    if (ret < 0)
        return ret;
    state = *converter;

    state->packet->data = (uint8_t *)input;
    state->packet->size = input_size;
    ret = avcodec_send_packet(state->codec, state->packet);
    state->packet->data = NULL;
    state->packet->size = 0;
    if (ret < 0)
        return ret;

    ret = avcodec_receive_frame(state->codec, state->frame);
    if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF)
        return 0;
    if (ret < 0)
        return ret;

    state->sws = sws_getCachedContext(state->sws,
                                      state->frame->width,
                                      state->frame->height,
                                      state->frame->format,
                                      width, height, AV_PIX_FMT_RGB24,
                                      SWS_BILINEAR, NULL, NULL, NULL);
    if (!state->sws)
        return AVERROR(ENOMEM);

    ret = sws_scale(state->sws,
                    (const uint8_t *const *)state->frame->data,
                    state->frame->linesize, 0, state->frame->height,
                    destination, destination_stride);
    if (ret != (int)height)
        return AVERROR(EIO);

    return width * height * 3;
#else
    (void)converter;
    (void)pixformat;
    (void)input;
    (void)input_size;
    (void)output;
    (void)width;
    (void)height;
    return -ENOTSUP;
#endif
}

void img_converter_free(img_converter_t *converter)
{
#ifdef HAVE_FFMPEG
    if (!converter)
        return;

    sws_freeContext(converter->sws);
    av_packet_free(&converter->packet);
    av_frame_free(&converter->frame);
    avcodec_free_context(&converter->codec);
    g_free(converter);
#else
    (void)converter;
#endif
}
