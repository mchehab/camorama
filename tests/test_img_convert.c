// SPDX-License-Identifier: GPL-2.0-or-later
/*
* Copyright (C) 2026 Mauro Carvalho Chehab <mchehab+huawei@kernel.org>
*/

#include <libgen.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>

#include "unittest.h"

#include "src/v4l.h"
#include "src/img_convert.h"

struct raw_imgs{
    char *fname;
    uint32_t fourcc;
};

#define COLOR_BAR_WIDTH  330
#define COLOR_BAR_HEIGHT 186

static struct raw_imgs rgb_imgs[] = {
    { .fname = "color_bars_RGB24.raw",   .fourcc = V4L2_PIX_FMT_RGB24},
    { .fname = "color_bars_BGR24.raw",   .fourcc = V4L2_PIX_FMT_BGR24},
};

static cam_t cam_rgb24 = { 0 };
static struct colorspace_parms colspace = { 0 };
static unsigned char *rgb24_buffer = NULL;
static const unsigned int rgb24_size = COLOR_BAR_HEIGHT * COLOR_BAR_WIDTH * 3;

unsigned char *load_raw_file(struct raw_imgs img_files[],
                             unsigned img_size, uint32_t fourcc,
                             cam_t *cam, uint32_t bitsperpixel)
{
    char *dir, *full_name;
    size_t size, read_size;
    unsigned char *buffer;
    char exe[PATH_MAX];
    unsigned int rc, i;
    FILE *fp;

    for (i = 0; i < img_size; i++)
        if (img_files[i].fourcc == fourcc)
            break;

    assert_int_not_equal(i, img_size);

    size = readlink("/proc/self/exe", exe, sizeof(exe) - 1);
    assert_true(size > 0);

    exe[size] = '\0';
    dir = dirname(strdup(exe));

    rc = asprintf(&full_name, "%s/%s", dir, img_files[i].fname);
    assert_true(rc > 0);

    fp = fopen(full_name, "rb");
    assert_non_null_msg(fp, full_name);

    fseek(fp, 0, SEEK_END);
    size = ftell(fp);

    assert_true(size > 0);

    rewind(fp);

    buffer = malloc(size);
    assert_non_null(buffer);

    read_size = fread(buffer, 1, size, fp);
    assert_int_equal(read_size, size);

    fclose(fp);


    cam->pixformat = img_files[i].fourcc;
    cam->colorspc = colspace;
    cam->width = COLOR_BAR_WIDTH;
    cam->height = COLOR_BAR_HEIGHT;

    cam->bytesperline = bitsperpixel * cam->width / 8;

    /* Allocate space to store the converted RGB24 image */
    cam->pic_buf = calloc(COLOR_BAR_HEIGHT, COLOR_BAR_WIDTH * 3);

    return buffer;
}

/*
 * Test set
 */

void test_bgr24(void **)
{
    unsigned int i, errors = 0;
    unsigned char *buffer;
    int rc;

    /* There are just two rgb formats, so we don't need a loop */
    cam_t cam_bgr24;

    buffer = load_raw_file(&rgb_imgs[1], 1, V4L2_PIX_FMT_BGR24,
                           &cam_bgr24, 24);
    assert_non_null(buffer);

    rc = img_convert_to_rgb24(&cam_bgr24, buffer);
    assert_int_not_equal(rc, 0);

    /* As RGB24 <=> BGR24 conversion is lossless, a simple comparision is OK */
    for  (i = 0; i < rgb24_size; i++)
        if (rgb24_buffer[i] != cam_bgr24.pic_buf[i])
            errors++;

    assert_int_equal(errors, 0);

    free(buffer);
}

/*
* Unit test runner
*/

static const struct CMUnitTest tests[] = {
    cmocka_unit_test(test_bgr24),
};

static int group_setup(void **)
{
    rgb24_buffer = load_raw_file(&rgb_imgs[0], 1, V4L2_PIX_FMT_RGB24,
                                 &cam_rgb24, 24);
    return 0;
}

static int group_teardown(void **)
{
    free(rgb24_buffer);

    return 0;
}

int test_img_convert(void)
{
    printf("Running img_convert tests.\n");
    return _cmocka_run_group_tests("img_convert",
                    tests,
                    ARRAY_SIZE(tests),
                    group_setup,
                    group_teardown);
}

REGISTER_TEST(test_img_convert , 0);
