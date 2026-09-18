/*
 * Bayer conversion imported from libv4lconvert in v4l-utils.
 *
 *             (C) 2008 Hans de Goede <hdegoede@redhat.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Suite 500, Boston, MA  02110-1335  USA
 *
 * Note: original bayer_to_bgr24 code from :
 * 1394-Based Digital Camera Control Library
 *
 * Bayer pattern decoding functions
 *
 * Written by Damien Douxchamps and Frederic Devernay
 *
 * Note that the original bayer.c in libdc1394 supports many different
 * bayer decode algorithms, for lib4lconvert the one in this file has been
 * chosen (and optimized a bit) and the other algorithms have been removed,
 * see bayer.c from libdc1394 for all supported algorithms
 */

#include <linux/videodev2.h>

#include "img_bayer.h"

/**************************************************************
 *     Color conversion functions for cameras that can        *
 * output raw-Bayer pattern images, such as some Basler and   *
 * Point Grey camera. Most of the algos presented here come   *
 * from http://www-ise.stanford.edu/~tingchen/ and have been  *
 * converted from Matlab to C and extended to all elementary  *
 * patterns.                                                  *
 **************************************************************/

/* inspired by OpenCV's Bayer decoding */
static void last_line_to_rgb24(const unsigned char *bayer,
                               const unsigned char *adjacent_bayer,
                               unsigned char *bgr, int width,
                               const int start_with_green,
                               const int blue_line)
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
                           unsigned char *bgr, int width, int height,
                           const unsigned int stride,
                           int start_with_green, int blue_line)
{
    /* render the first line */
    last_line_to_rgb24(bayer, bayer + stride, bgr, width,
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
    last_line_to_rgb24(bayer + stride, bayer, bgr, width,
                       !start_with_green, !blue_line);
}

void img_bayer_to_rgb24(const unsigned char *bayer,
                        unsigned char *bgr, int width, int height,
                        const unsigned int stride, unsigned int pixfmt)
{
    bayer_to_rgb24(bayer, bgr, width, height, stride,
                   pixfmt == V4L2_PIX_FMT_SGBRG8        /* start with green */
                   || pixfmt == V4L2_PIX_FMT_SGRBG8,
                   pixfmt != V4L2_PIX_FMT_SBGGR8        /* blue line */
                   && pixfmt != V4L2_PIX_FMT_SGBRG8);
}
