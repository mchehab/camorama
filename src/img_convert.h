/* SPDX-License-Identifier: GPL-2.0-or-later */
#ifndef CAMORAMA_IMG_CONVERT_H
#define CAMORAMA_IMG_CONVERT_H

#include <stddef.h>
#include <stdint.h>
#include <glib.h>
#include <linux/videodev2.h>

struct camera;
typedef struct img_converter img_converter_t;

struct img_format {
    unsigned int pixformat;
    unsigned int depth;
    int y_decimation;
    int x_decimation;
    unsigned int is_rgb:1;
};

const struct img_format *img_format_get(unsigned int pixformat);
unsigned int img_format_order(unsigned int pixformat);
unsigned int img_convert_to_rgb24(struct camera *cam,
                                  unsigned char *inbuf);
void img_get_colorspace_data(struct camera *cam,
                             struct v4l2_format *fmt);
gboolean img_codec_supported(unsigned int pixformat);
int img_decode_to_rgb24(img_converter_t **converter,
                        unsigned int pixformat,
                        const unsigned char *input, size_t input_size,
                        unsigned char *output,
                        unsigned int width, unsigned int height);
void img_converter_free(img_converter_t *converter);

#endif
