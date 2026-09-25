#include "config.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "img_bayer.h"
#include "img_convert.h"
#include "img_ffmpeg.h"
#include "camera-backend.h"

#ifndef V4L2_PIX_FMT_YUVA32
#define V4L2_PIX_FMT_YUVA32 v4l2_fourcc('Y', 'U', 'V', 'A')
#endif
#ifndef V4L2_PIX_FMT_YUVX32
#define V4L2_PIX_FMT_YUVX32 v4l2_fourcc('Y', 'U', 'V', 'X')
#endif

#define BYTE_CLAMP(a) CLAMP(a, 0, 255)

/*
 * Formats that are natively supported.
 *
 * Ordered by quality: better first.
 *
 * On experimental tests, YUYV and MJPEG are similar for simple images
 * like SMPTE color bars, but for more complex images, YuYV is probably
 * better.
 *
 * H.264 provides good quality, but as it may have B-frames, it could
 * introduce delays. So, place it at the end.
 */
const struct img_format supported_formats[] = {
    /* lossless formats */
    { V4L2_PIX_FMT_RGB24,   24, -1, -1,  IMG_COLORMAP_RGB },
    { V4L2_PIX_FMT_BGR24,   24, -1, -1,  IMG_COLORMAP_RGB },

    { V4L2_PIX_FMT_BGR32,   32, -1, -1,  IMG_COLORMAP_RGB },
    { V4L2_PIX_FMT_ABGR32,  32, -1, -1,  IMG_COLORMAP_RGB },
    { V4L2_PIX_FMT_XBGR32,  32, -1, -1,  IMG_COLORMAP_RGB },

    { V4L2_PIX_FMT_RGB32,   32, -1, -1,  IMG_COLORMAP_RGB },
    { V4L2_PIX_FMT_ARGB32,  32, -1, -1,  IMG_COLORMAP_RGB },
    { V4L2_PIX_FMT_XRGB32,  32, -1, -1,  IMG_COLORMAP_RGB },

    { V4L2_PIX_FMT_RGBA32,  32, -1, -1,  IMG_COLORMAP_RGB },
    { V4L2_PIX_FMT_RGBX32,  32, -1, -1,  IMG_COLORMAP_RGB },
    { V4L2_PIX_FMT_BGRA32,  32, -1, -1,  IMG_COLORMAP_RGB },
    { V4L2_PIX_FMT_BGRX32,  32, -1, -1,  IMG_COLORMAP_RGB },

    /* YUV formats up to 16 bits per pixel */
    { V4L2_PIX_FMT_YUV32,   32, -1, -1,  IMG_COLORMAP_YCBCR },
    { V4L2_PIX_FMT_AYUV32,  32, -1, -1,  IMG_COLORMAP_YCBCR },
    { V4L2_PIX_FMT_XYUV32,  32, -1, -1,  IMG_COLORMAP_YCBCR },
    { V4L2_PIX_FMT_VUYA32,  32, -1, -1,  IMG_COLORMAP_YCBCR },
    { V4L2_PIX_FMT_VUYX32,  32, -1, -1,  IMG_COLORMAP_YCBCR },
    { V4L2_PIX_FMT_YUVA32,  32, -1, -1,  IMG_COLORMAP_YCBCR },
    { V4L2_PIX_FMT_YUVX32,  32, -1, -1,  IMG_COLORMAP_YCBCR },

    { V4L2_PIX_FMT_YUV24,   24, -1, -1,  IMG_COLORMAP_YCBCR },

    { V4L2_PIX_FMT_YUYV,    16, -1, -1,  IMG_COLORMAP_YCBCR },
    { V4L2_PIX_FMT_UYVY,    16, -1, -1,  IMG_COLORMAP_YCBCR },
    { V4L2_PIX_FMT_YVYU,    16, -1, -1,  IMG_COLORMAP_YCBCR },
    { V4L2_PIX_FMT_VYUY,    16, -1, -1,  IMG_COLORMAP_YCBCR },

    { V4L2_PIX_FMT_YUV565,  16, -1, -1,  IMG_COLORMAP_YCBCR },
    { V4L2_PIX_FMT_YUV555,  16, -1, -1,  IMG_COLORMAP_YCBCR },
    { V4L2_PIX_FMT_YUV444,  16, -1, -1,  IMG_COLORMAP_YCBCR },

#ifdef HAVE_FFMPEG
    { V4L2_PIX_FMT_MJPEG,    0, -1, -1 , IMG_COLORMAP_JPEG },
    { V4L2_PIX_FMT_JPEG,     0, -1, -1 , IMG_COLORMAP_JPEG },
#endif

    /* lossy decoding RGB Formats */
    { V4L2_PIX_FMT_RGB565,  16, -1, -1,  IMG_COLORMAP_RGB },
    { V4L2_PIX_FMT_RGB565X, 16, -1, -1,  IMG_COLORMAP_RGB },
    { V4L2_PIX_FMT_RGB555,  16, -1, -1,  IMG_COLORMAP_RGB },
    { V4L2_PIX_FMT_ARGB555, 16, -1, -1,  IMG_COLORMAP_RGB },
    { V4L2_PIX_FMT_XRGB555, 16, -1, -1,  IMG_COLORMAP_RGB },
    { V4L2_PIX_FMT_RGBA555, 16, -1, -1,  IMG_COLORMAP_RGB },
    { V4L2_PIX_FMT_RGBX555, 16, -1, -1,  IMG_COLORMAP_RGB },
    { V4L2_PIX_FMT_ABGR555, 16, -1, -1,  IMG_COLORMAP_RGB },
    { V4L2_PIX_FMT_XBGR555, 16, -1, -1,  IMG_COLORMAP_RGB },
    { V4L2_PIX_FMT_BGRA555, 16, -1, -1,  IMG_COLORMAP_RGB },
    { V4L2_PIX_FMT_BGRX555, 16, -1, -1,  IMG_COLORMAP_RGB },
    { V4L2_PIX_FMT_RGB555X, 16, -1, -1,  IMG_COLORMAP_RGB },
    { V4L2_PIX_FMT_RGB444,  16, -1, -1,  IMG_COLORMAP_RGB },
    { V4L2_PIX_FMT_ARGB444, 16, -1, -1,  IMG_COLORMAP_RGB },
    { V4L2_PIX_FMT_XRGB444, 16, -1, -1,  IMG_COLORMAP_RGB },
    { V4L2_PIX_FMT_RGBA444, 16, -1, -1,  IMG_COLORMAP_RGB },
    { V4L2_PIX_FMT_RGBX444, 16, -1, -1,  IMG_COLORMAP_RGB },
    { V4L2_PIX_FMT_ABGR444, 16, -1, -1,  IMG_COLORMAP_RGB },
    { V4L2_PIX_FMT_XBGR444, 16, -1, -1,  IMG_COLORMAP_RGB },
    { V4L2_PIX_FMT_BGRA444, 16, -1, -1,  IMG_COLORMAP_RGB },
    { V4L2_PIX_FMT_BGRX444, 16, -1, -1,  IMG_COLORMAP_RGB },
    { V4L2_PIX_FMT_RGB332,   8, -1, -1,  IMG_COLORMAP_RGB },

    /* Semi-planar YUV formats (12-bits average) */
    { V4L2_PIX_FMT_NV12,     8,  1, -1,  IMG_COLORMAP_YCBCR },
    { V4L2_PIX_FMT_NV21,     8,  1, -1,  IMG_COLORMAP_YCBCR },
    { V4L2_PIX_FMT_NV16,     8,  0, -1,  IMG_COLORMAP_YCBCR },
    { V4L2_PIX_FMT_NV61,     8,  0, -1,  IMG_COLORMAP_YCBCR },
    { V4L2_PIX_FMT_YUV420,   8,  1 , 1,  IMG_COLORMAP_YCBCR },
    { V4L2_PIX_FMT_YVU420,   8,  1,  1,  IMG_COLORMAP_YCBCR },
    { V4L2_PIX_FMT_YUV422P,  8,  0,  1,  IMG_COLORMAP_YCBCR },

    /* Bayer formats */
    { V4L2_PIX_FMT_SBGGR8,   8, -1, -1,  IMG_COLORMAP_OTHER },
    { V4L2_PIX_FMT_SGBRG8,   8, -1, -1,  IMG_COLORMAP_OTHER },
    { V4L2_PIX_FMT_SGRBG8,   8, -1, -1,  IMG_COLORMAP_OTHER },
    { V4L2_PIX_FMT_SRGGB8,   8, -1, -1,  IMG_COLORMAP_OTHER },

#ifdef HAVE_FFMPEG
    { V4L2_PIX_FMT_H264,     0, -1, -1,  IMG_COLORMAP_YCBCR },
#endif

    /*
     * Grey formats - lowest priority as those usually require forcing it
     */
    { V4L2_PIX_FMT_GREY,     8, -1, -1,  IMG_COLORMAP_OTHER },
    { V4L2_PIX_FMT_Y10,     16, -1, -1,  IMG_COLORMAP_OTHER },
    { V4L2_PIX_FMT_Y12,     16, -1, -1,  IMG_COLORMAP_OTHER },
    { V4L2_PIX_FMT_Y16,     16, -1, -1,  IMG_COLORMAP_OTHER },
    { V4L2_PIX_FMT_Y16_BE,  16, -1, -1,  IMG_COLORMAP_OTHER },

};

const size_t supported_formats_count = ARRAY_SIZE(supported_formats);

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
    case V4L2_PIX_FMT_RGB332:
        for (i = 0; i < 2; i++) {
            unsigned char p = plane0[i];

            *(*dst)++ = ((p >> 5) & 0x07) * 255 / 7;
            *(*dst)++ = ((p >> 2) & 0x07) * 255 / 7;
            *(*dst)++ = (p & 0x03) * 255 / 3;
        }
        break;
    case V4L2_PIX_FMT_RGB444:
    case V4L2_PIX_FMT_ARGB444:
    case V4L2_PIX_FMT_XRGB444:
    case V4L2_PIX_FMT_RGBA444:
    case V4L2_PIX_FMT_RGBX444:
    case V4L2_PIX_FMT_ABGR444:
    case V4L2_PIX_FMT_XBGR444:
    case V4L2_PIX_FMT_BGRA444:
    case V4L2_PIX_FMT_BGRX444:
        for (i = 0; i < 2; i++) {
            unsigned int r, g, b;

            pix = plane0[0] | (plane0[1] << 8);
            switch (cam->pixformat) {
            case V4L2_PIX_FMT_RGB444:
            case V4L2_PIX_FMT_ARGB444:
            case V4L2_PIX_FMT_XRGB444:
                r = (pix >> 8) & 0xf;
                g = (pix >> 4) & 0xf;
                b = pix & 0xf;
                break;
            case V4L2_PIX_FMT_RGBA444:
            case V4L2_PIX_FMT_RGBX444:
                r = (pix >> 12) & 0xf;
                g = (pix >> 8) & 0xf;
                b = (pix >> 4) & 0xf;
                break;
            case V4L2_PIX_FMT_ABGR444:
            case V4L2_PIX_FMT_XBGR444:
                r = pix & 0xf;
                g = (pix >> 4) & 0xf;
                b = (pix >> 8) & 0xf;
                break;
            default:
                r = (pix >> 4) & 0xf;
                g = (pix >> 8) & 0xf;
                b = (pix >> 12) & 0xf;
                break;
            }
            *(*dst)++ = r * 17;
            *(*dst)++ = g * 17;
            *(*dst)++ = b * 17;
            plane0 += 2;
        }
        break;
    case V4L2_PIX_FMT_RGB555:
    case V4L2_PIX_FMT_ARGB555:
    case V4L2_PIX_FMT_XRGB555:
    case V4L2_PIX_FMT_RGBA555:
    case V4L2_PIX_FMT_RGBX555:
    case V4L2_PIX_FMT_ABGR555:
    case V4L2_PIX_FMT_XBGR555:
    case V4L2_PIX_FMT_BGRA555:
    case V4L2_PIX_FMT_BGRX555:
    case V4L2_PIX_FMT_RGB555X:
        for (i = 0; i < 2; i++) {
            unsigned int r, g, b;

            pix = (cam->pixformat == V4L2_PIX_FMT_RGB555X) ?
                  (plane0[0] << 8) | plane0[1] : plane0[0] | (plane0[1] << 8);
            switch (cam->pixformat) {
            case V4L2_PIX_FMT_RGB555:
            case V4L2_PIX_FMT_ARGB555:
            case V4L2_PIX_FMT_XRGB555:
            case V4L2_PIX_FMT_RGB555X:
                r = (pix >> 10) & 0x1f;
                g = (pix >> 5) & 0x1f;
                b = pix & 0x1f;
                break;
            case V4L2_PIX_FMT_RGBA555:
            case V4L2_PIX_FMT_RGBX555:
                r = (pix >> 11) & 0x1f;
                g = (pix >> 6) & 0x1f;
                b = (pix >> 1) & 0x1f;
                break;
            case V4L2_PIX_FMT_ABGR555:
            case V4L2_PIX_FMT_XBGR555:
                r = pix & 0x1f;
                g = (pix >> 5) & 0x1f;
                b = (pix >> 10) & 0x1f;
                break;
            default:
                r = (pix >> 1) & 0x1f;
                g = (pix >> 6) & 0x1f;
                b = (pix >> 11) & 0x1f;
                break;
            }
            *(*dst)++ = (r << 3) | (r >> 2);
            *(*dst)++ = (g << 3) | (g >> 2);
            *(*dst)++ = (b << 3) | (b >> 2);
            plane0 += 2;
        }
        break;
    case V4L2_PIX_FMT_GREY:
    case V4L2_PIX_FMT_Y10:
    case V4L2_PIX_FMT_Y12:
    case V4L2_PIX_FMT_Y16:
    case V4L2_PIX_FMT_Y16_BE:
        for (i = 0; i < 2; i++) {
            unsigned int y;

            if (cam->pixformat == V4L2_PIX_FMT_GREY)
                y = plane0[0];
            else {
                pix = cam->pixformat == V4L2_PIX_FMT_Y16_BE ?
                      (plane0[0] << 8) | plane0[1] : plane0[0] | (plane0[1] << 8);
                if (cam->pixformat == V4L2_PIX_FMT_Y10)
                    y = pix >> 2;
                else if (cam->pixformat == V4L2_PIX_FMT_Y12)
                    y = pix >> 4;
                else
                    y = pix >> 8;
            }
            *(*dst)++ = y;
            *(*dst)++ = y;
            *(*dst)++ = y;
            plane0 += cam->pixformat == V4L2_PIX_FMT_GREY ? 1 : 2;
        }
        break;
    case V4L2_PIX_FMT_RGB565X: /* rrrrrggg gggbbbbb */
        for (i = 0; i < 2; i++) {
            pix = (plane0[0] << 8) + plane0[1];

            *(*dst)++ = (unsigned char)(((pix & 0xf800) >> 11) << 3) | 0x07;
            *(*dst)++ = (unsigned char)((((pix & 0x07e0) >> 5)) << 2) | 0x03;
            *(*dst)++ = (unsigned char)((pix & 0x1f) << 3) | 0x07;

            plane0 += 2;
        }
        break;
    case V4L2_PIX_FMT_RGB565: /* gggbbbbb rrrrrggg */
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
    case V4L2_PIX_FMT_YUV444:
    case V4L2_PIX_FMT_YUV555:
    case V4L2_PIX_FMT_YUV565:
        for (i = 0; i < 2; i++) {
            int32_t y, u, v;

            pix = plane0[0] | (plane0[1] << 8);
            if (cam->pixformat == V4L2_PIX_FMT_YUV444) {
                y = ((pix >> 8) & 0x0f) * 17;
                u = ((pix >> 4) & 0x0f) * 17;
                v = (pix & 0x0f) * 17;
            } else if (cam->pixformat == V4L2_PIX_FMT_YUV555) {
                y = ((pix >> 10) & 0x1f) * 255 / 31;
                u = ((pix >> 5) & 0x1f) * 255 / 31;
                v = (pix & 0x1f) * 255 / 31;
            } else {
                y = ((pix >> 11) & 0x1f) * 255 / 31;
                u = ((pix >> 5) & 0x3f) * 255 / 63;
                v = (pix & 0x1f) * 255 / 31;
            }
            convert_yuv(c, y, u, v, dst);
            plane0 += 2;
        }
        break;
    case V4L2_PIX_FMT_YUV24:
        for (i = 0; i < 2; i++) {
            convert_yuv(c, plane0[0], plane0[1], plane0[2], dst);
            plane0 += 3;
        }
        break;
    case V4L2_PIX_FMT_YUV32:
    case V4L2_PIX_FMT_AYUV32:
    case V4L2_PIX_FMT_XYUV32:
        for (i = 0; i < 2; i++) {
            convert_yuv(c, plane0[1], plane0[2], plane0[3], dst);
            plane0 += 4;
        }
        break;
    case V4L2_PIX_FMT_VUYA32:
    case V4L2_PIX_FMT_VUYX32:
        for (i = 0; i < 2; i++) {
            convert_yuv(c, plane0[2], plane0[1], plane0[0], dst);
            plane0 += 4;
        }
        break;
    case V4L2_PIX_FMT_YUVA32:
    case V4L2_PIX_FMT_YUVX32:
        for (i = 0; i < 2; i++) {
            convert_yuv(c, plane0[0], plane0[1], plane0[2], dst);
            plane0 += 4;
        }
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
    case V4L2_PIX_FMT_RGBA32:
    case V4L2_PIX_FMT_RGBX32:
        for (i = 0; i < 2; i++) {
            *(*dst)++ = plane0[0];
            *(*dst)++ = plane0[1];
            *(*dst)++ = plane0[2];
            plane0 += 4;
        }
        break;
    case V4L2_PIX_FMT_BGRA32:
    case V4L2_PIX_FMT_BGRX32:
        for (i = 0; i < 2; i++) {
            *(*dst)++ = plane0[3];
            *(*dst)++ = plane0[2];
            *(*dst)++ = plane0[1];
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
    case V4L2_PIX_FMT_BGR24:
        for (i = 0; i < 2; i++) {
            *(*dst)++ = plane0[2];
            *(*dst)++ = plane0[1];
            *(*dst)++ = plane0[0];

            plane0 += 3;
        }
        break;
    /*
     * We need to handle RGB24 due to padding issues on some cameras
     */
    default:
    case V4L2_PIX_FMT_RGB24:
        for (i = 0; i < 2; i++) {
            *(*dst)++ = plane0[0];
            *(*dst)++ = plane0[1];
            *(*dst)++ = plane0[2];

            plane0 += 3;
        }
        break;
    }
}

unsigned int img_convert_to_rgb24(cam_t *cam, unsigned char *inbuf,
                                  size_t input_size, unsigned char *display_data)
{
    unsigned char *plane0 = inbuf;
    unsigned char *p_out = display_data;
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
        return 0;

    /*
     * It is tempting to add a fast logic for RGB and BGR, but this won't
     * work, as some cameras may have bigger bytesperline than expected,
     * as they could be adding per-line padding filling at the end.
     */
    switch (cam->pixformat) {
    case V4L2_PIX_FMT_SBGGR8:
    case V4L2_PIX_FMT_SGBRG8:
    case V4L2_PIX_FMT_SGRBG8:
    case V4L2_PIX_FMT_SRGGB8:
        if (width < 3 || height < 2 || bytesperline < width)
            return 0;
        img_bayer_to_rgb24(inbuf, display_data, width, height,
                           bytesperline, cam->pixformat);
        return width * height * 3;
#ifdef HAVE_FFMPEG
    case V4L2_PIX_FMT_MJPEG:
    case V4L2_PIX_FMT_JPEG:
    case V4L2_PIX_FMT_H264:
        return img_ffmpeg_to_rgb24(&cam->converter, cam->pixformat,
                                   inbuf, input_size, display_data,
                                   width, height);
#endif
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
        plane0_size = bytesperline * height;
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
    enum v4l2_colorspace default_colorspace;
    gboolean is_rgb = FALSE;
    gboolean is_ycbcr = FALSE;

    memset(c, 0, sizeof(*c));

    video_fmt = img_format_get(fmt->fmt.pix.pixelformat);
    if (!video_fmt)
            return;

    switch (video_fmt->colormap) {
    case IMG_COLORMAP_RGB:
        default_colorspace = V4L2_COLORSPACE_SRGB;
        is_rgb = TRUE;
        break;
    case IMG_COLORMAP_YCBCR:
        default_colorspace = V4L2_COLORSPACE_SRGB;
        is_ycbcr = TRUE;
        break;
    case IMG_COLORMAP_JPEG:
        default_colorspace = V4L2_COLORSPACE_JPEG;
        is_ycbcr = TRUE;
        break;
    case IMG_COLORMAP_OTHER:
    default:
        default_colorspace = V4L2_COLORSPACE_SRGB;
        break;
    }

    /* Use the driver-returned colorspace when available. */
    if (fmt->fmt.pix.colorspace != V4L2_COLORSPACE_DEFAULT) {
        c->colorspace = fmt->fmt.pix.colorspace;
    } else {
        c->colorspace = default_colorspace;
    }

    if (fmt->fmt.pix.xfer_func == V4L2_XFER_FUNC_DEFAULT)
        c->xfer_func = V4L2_MAP_XFER_FUNC_DEFAULT(c->colorspace);
    else
        c->xfer_func = fmt->fmt.pix.xfer_func;

    if (is_ycbcr) {
        if (fmt->fmt.pix.ycbcr_enc == V4L2_YCBCR_ENC_DEFAULT)
            c->ycbcr_enc = V4L2_MAP_YCBCR_ENC_DEFAULT(c->colorspace);
        else
            c->ycbcr_enc = fmt->fmt.pix.ycbcr_enc;
    }

    if (fmt->fmt.pix.quantization == V4L2_QUANTIZATION_DEFAULT) {
        c->quantization = V4L2_MAP_QUANTIZATION_DEFAULT(is_rgb,
                                                        c->colorspace,
                                                        c->ycbcr_enc);
    } else {
        c->quantization = fmt->fmt.pix.quantization;
    }

    if (cam->debug == TRUE) {
        if (is_ycbcr) {
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
