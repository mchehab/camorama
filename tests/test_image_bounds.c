// SPDX-License-Identifier: GPL-2.0-or-later

#include "config.h"
#include "camera-backend.h"
#include "img_convert.h"
#include "fileio.h"
#include "filter.h"

#include <stdarg.h>
#include <setjmp.h>
#include <cmocka.h>
#include "unittest.h"

static void test_small_images(void **state)
{
    (void)state;
    GType types[] = { camorama_filter_smooth_get_type(),
                     camorama_filter_sobel_get_type(),
                     camorama_filter_wacky_get_type(),
                     camorama_filter_laplace_get_type(),
                     camorama_filter_reichardt_get_type() };
    for (unsigned int t = 0; t < G_N_ELEMENTS(types); t++) {
        CamoramaFilter *filter = g_object_new(types[t], NULL);
        for (int h = 1; h <= 16; h++) {
            for (int w = 1; w <= 16; w++) {
                guchar *pixels = g_malloc(w * h * 3);
                memset(pixels, 127, w * h * 3);
                camorama_filter_apply(filter, pixels, w, h, 3);
                add_rgb_text(pixels, w, h, "\xc3\xa9 very long label", "%c", TRUE, TRUE);
                g_free(pixels);
            }
        }
        g_object_unref(filter);
    }
}

static void test_truncated_raw_frames(void **state)
{
    cam_t cam = { 0 };
    unsigned char out[192];
    (void)state;

    cam.width = cam.height = 8;
    for (size_t f = 0; f < supported_formats_count; f++) {
        const struct img_format *fmt = &supported_formats[f];
        if (!fmt->depth)
            continue;
        cam.pixformat = fmt->pixformat;
        cam.bytesperline = 8 * fmt->depth / 8;
        size_t size = cam.bytesperline * 8;
        if (fmt->y_decimation >= 0) {
            if (fmt->x_decimation >= 0)
                size += 2 * (size >> (fmt->y_decimation + fmt->x_decimation));
            else
                size += size >> fmt->y_decimation;
        }
        for (size_t n = 0; n <= size; n++) {
            guchar *pixels = g_malloc(MAX(n, 1));
            memset(pixels, 127, MAX(n, 1));
            unsigned int ret = img_convert_to_rgb24(&cam, pixels, n, out);
            assert_int_equal(ret, n == size ? sizeof(out) : 0);
            g_free(pixels);
        }
    }
}

static int test_image_bounds(void)
{
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(test_small_images),
        cmocka_unit_test(test_truncated_raw_frames),
    };

    return _cmocka_run_group_tests("image_bounds", tests,
                                   G_N_ELEMENTS(tests), NULL, NULL);
}

REGISTER_TEST(test_image_bounds, 0);
