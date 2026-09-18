/* SPDX-License-Identifier: GPL-2.0-or-later */
#ifndef IMG_BAYER_H
#define IMG_BAYER_H
void img_bayer_to_rgb24(const unsigned char *bayer,
                        unsigned char *bgr, int width, int height,
                        const unsigned int stride, unsigned int pixfmt);
#endif
