#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "img_convert.h"
#include "v4l.h"

#define BYTE_CLAMP(a) CLAMP(a, 0, 255)

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
};

#define ARRAY_SIZE(a)  (sizeof(a)/sizeof(*a))

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

unsigned int img_convert_to_rgb24(cam_t *cam, unsigned char *inbuf)
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
        return 0;

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
