/* SPDX-License-Identifier: GPL-2.0-or-later */
#ifndef CAMORAMA_IMG_CONVERT_H
#define CAMORAMA_IMG_CONVERT_H

#include <linux/videodev2.h>

struct camera;

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

#endif
