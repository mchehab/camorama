// SPDX-License-Identifier: GPL-2.0-or-later
/*
* Copyright (C) 2026 Mauro Carvalho Chehab <mchehab+huawei@kernel.org>
*/

#define _GNU_SOURCE

#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>

#include "unittest.h"

#include "src/camera-backend.h"
#include "src/img_convert.h"
#include "test_utils.h"

#define COLOR_BAR_WIDTH  330
#define COLOR_BAR_HEIGHT 186

/* TODO: Peak S/N ratio limits should likely be placed at raw_imgs table */
#define PSNR_GOAL 26.0 /* dB */

struct raw_imgs {
    char *name;
    uint32_t fourcc;
    int bits; /* per pixel */
    const char *av_fmt;
    char *av_extra;

    char *fname;
    double psnr;
};

static struct raw_imgs exact_imgs[] = {
    { .name = "RGB24",   .fourcc = V4L2_PIX_FMT_RGB24,   .bits = 24, .av_fmt = "rgb24"},

    { .name = "BGR24",   .fourcc = V4L2_PIX_FMT_BGR24,   .bits = 24, .av_fmt = "bgr24"},

    { .name = "BGR32",   .fourcc = V4L2_PIX_FMT_BGR32,   .bits = 32, .av_fmt = "bgr0"},
    { .name = "ABGR32",  .fourcc = V4L2_PIX_FMT_ABGR32,  .bits = 32, .av_fmt = "bgra"},
    { .name = "XBGR32",  .fourcc = V4L2_PIX_FMT_XBGR32,  .bits = 32, .av_fmt = "bgr0"},

    { .name = "RGB32",   .fourcc = V4L2_PIX_FMT_RGB32,   .bits = 32, .av_fmt = "argb"},
    { .name = "ARGB32",  .fourcc = V4L2_PIX_FMT_ARGB32,  .bits = 32, .av_fmt = "argb"},
    { .name = "XRGB32",  .fourcc = V4L2_PIX_FMT_XRGB32,  .bits = 32, .av_fmt = "0rgb"},
};

static struct raw_imgs aprox_imgs[] = {
    { .name = "UYVY",    .fourcc = V4L2_PIX_FMT_UYVY,    .bits = 16, .av_fmt = "uyvy422"},
    { .name = "VYUY",    .fourcc = V4L2_PIX_FMT_VYUY,    .bits = 16, .av_fmt = "uyvy422",  .av_extra = "-vf format=yuv422p,swapuv" },
    { .name = "YUYV",    .fourcc = V4L2_PIX_FMT_YUYV,    .bits = 16, .av_fmt = "yuyv422"},
    { .name = "YVYU",    .fourcc = V4L2_PIX_FMT_YVYU,    .bits = 16, .av_fmt = "yvyu422"},

    /* For semi-planar objects, bits is for the Y plane */
    { .name = "YUV422P", .fourcc = V4L2_PIX_FMT_YUV422P, .bits = 8,  .av_fmt = "yuv422p"},
    { .name = "NV12",    .fourcc = V4L2_PIX_FMT_NV12,    .bits = 8,  .av_fmt = "nv12"},
    { .name = "NV21",    .fourcc = V4L2_PIX_FMT_NV21,    .bits = 8,  .av_fmt = "nv21"},
    { .name = "NV16",    .fourcc = V4L2_PIX_FMT_NV16,    .bits = 8,  .av_fmt = "nv16"},
    { .name = "NV61",    .fourcc = V4L2_PIX_FMT_NV61,    .bits = 8,  .av_fmt = "nv16",     .av_extra = "-vf format=nv16,swapuv" },
    { .name = "YUV420",  .fourcc = V4L2_PIX_FMT_YUV420,  .bits = 8,  .av_fmt = "yuv420p"},
    { .name = "YVU420",  .fourcc = V4L2_PIX_FMT_YVU420,  .bits = 8,  .av_fmt = "yuv420p",  .av_extra = "-vf format=yuv420p,swapuv" },

    { .name = "RGB565",  .fourcc = V4L2_PIX_FMT_RGB565,  .bits = 16, .av_fmt = "rgb565le"},
    { .name = "RGB565X", .fourcc = V4L2_PIX_FMT_RGB565X, .bits = 16, .av_fmt = "rgb565be"},

    { .name = "MJPEG",   .fourcc = V4L2_PIX_FMT_MJPEG,               .av_fmt = "yuvj422p", .av_extra = "-c:v mjpeg" },
    { .name = "H264",    .fourcc = V4L2_PIX_FMT_H264,                .av_fmt = "yuv420p" },
};

static cam_t cam_rgb24 = { 0 };
static struct colorspace_parms colspace = { 0 };
static unsigned char *rgb24_buffer = NULL;
static const unsigned int rgb24_size = COLOR_BAR_HEIGHT * COLOR_BAR_WIDTH * 3;
static bool nv16_supported = true;

static int color_bars_generate(struct raw_imgs *img)
{
    char *dir, *png_file, *fname, *extra = NULL;
    const char *codec;
    int argc, status;
    char *argv[32];
    struct stat st;
    int rc;

    dir = test_get_executable_dir();
    assert_non_null(dir);
    rc = asprintf(&png_file, "%s/%s", dir, PNG_FILE);
    assert_true(rc > 0);
    rc = asprintf(&fname, "%s/color_bars_%s.raw", dir, img->name);
    assert_true(rc > 0);
    free(dir);

    {
        argc = 0;
        argv[argc++] = (char *)FFMPEG_BIN;
        argv[argc++] = "-loglevel";
        argv[argc++] = (!strcmp(img->name, "NV16") ||
                        !strcmp(img->name, "NV61")) ? "quiet" : "error";
        argv[argc++] = "-y";
        argv[argc++] = "-i";
        argv[argc++] = png_file;
        argv[argc++] = "-pix_fmt";
        argv[argc++] = (char *)img->av_fmt;

        if (!strcmp(img->name, "H264")) {
            const char *encoder = test_ffmpeg_h264_encoder(false);

            if (!encoder) {
                free(png_file);
                free(fname);
                return -ENOTSUP;
            }
            argv[argc++] = "-c:v";
            argv[argc++] = (char *)encoder;
            argv[argc++] = "-bf";
            argv[argc++] = "0";
        }

        if (img->av_extra) {
            extra = strdup(img->av_extra);
            if (!extra) {
                free(png_file);
                free(fname);
                return -ENOMEM;
            }

            char *saveptr = NULL;
            char *tok = strtok_r(extra, " ", &saveptr);

            while (tok && argc < (int)(ARRAY_SIZE(argv) - 4)) {
                argv[argc++] = tok;
                tok = strtok_r(NULL, " ", &saveptr);
            }
            if (tok) {
                free(extra);
                free(png_file);
                free(fname);
                return -E2BIG;
            }
        }

        if (!strcmp(img->name, "H264"))
            codec = "h264";
        else if (!strcmp(img->name, "MJPEG"))
            codec = "mjpeg";
        else
            codec = "rawvideo";

        argv[argc++] = "-f";
        argv[argc++] = (char *)codec;
        argv[argc++] = fname;
        argv[argc] = NULL;

    }

    status = test_run_program(argv);
    free(extra);
    free(png_file);
    if (status) {
        free(fname);
        return -EIO;
    }
    assert_false(stat(fname, &st));
    assert_true(S_ISREG(st.st_mode));
    assert_int_not_equal(st.st_size, 0);

    img->fname = fname;
    return 0;
}

static unsigned char *load_raw_file(struct raw_imgs *img, cam_t *cam)
{
    size_t size, read_size;
    unsigned char *buffer;
    struct stat st;
    char *fname;
    FILE *fp;
    int rc;

    fname = img->fname;

    rc = stat(fname, &st);
    assert_true(rc == 0);

    if (!S_ISREG(st.st_mode)) {
        skip();
    }

    fp = fopen(fname, "rb");
    assert_non_null_msg(fp, fname);

    fseek(fp, 0, SEEK_END);
    size = ftell(fp);

    assert_true(size > 0);

    rewind(fp);

    buffer = malloc(size);
    assert_non_null(buffer);

    read_size = fread(buffer, 1, size, fp);
    assert_int_equal(read_size, size);

    fclose(fp);


    cam->pixformat = img->fourcc;
    cam->colorspc = colspace;
    cam->width = COLOR_BAR_WIDTH;
    cam->height = COLOR_BAR_HEIGHT;
    cam->converter = NULL;
    cam->bytesperline = img->bits * cam->width / 8;
    cam->sizeimage = size;

    /* Allocate space to store the converted RGB24 image */
    cam->display_buffers[0].data = calloc(COLOR_BAR_HEIGHT, COLOR_BAR_WIDTH * 3);

    return buffer;
}

/*
 * Test set
 */

/* Test exact match between two images */
void test_lossless(void **state)
{
    struct raw_imgs *img = *state;
    unsigned int i, errors = 0;
    unsigned char *buffer;
    int rc;

    cam_t cam;

    buffer = load_raw_file(img, &cam);
    assert_non_null(buffer);

    rc = img_convert_to_rgb24(&cam, buffer, cam.sizeimage,
                              cam.display_buffers[0].data);
    assert_int_not_equal(rc, 0);

    for  (i = 0; i < rgb24_size; i++)
        if (rgb24_buffer[i] != cam.display_buffers[0].data[i])
            errors++;

    // FIXME: add a command line arg to enable it
    assert_int_equal(test_save_png_named(img->name, cam.display_buffers[0].data, cam.width,
                                         cam.height, 0), 0);

    assert_int_equal(errors, 0);

    free(cam.display_buffers[0].data);
    free(buffer);
}

/* Test for exact match between two images */
void test_psnr(void **state)
{
    struct raw_imgs *img = *state;
    unsigned char *buffer;
    int rc;

    cam_t cam;

    buffer = load_raw_file(img, &cam);
    assert_non_null(buffer);

    rc = img_convert_to_rgb24(&cam, buffer, cam.sizeimage,
                              cam.display_buffers[0].data);
    assert_int_not_equal(rc, 0);

    img->psnr = test_estimate_psnr(rgb24_buffer, cam.display_buffers[0].data,
                                   rgb24_size);

    // FIXME: add a command line arg to enable it
    assert_int_equal(test_save_png_named(img->name, cam.display_buffers[0].data, cam.width,
                                         cam.height, 0), 0);

    /* Should be OK for 12 bits YUV */
    assert_true(img->psnr >= PSNR_GOAL);

    free(cam.display_buffers[0].data);
    free(buffer);
}


/*
* Unit test runner
*/

static int group_setup(void **)
{
    struct v4l2_format fmt = { 0 };

    fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    fmt.fmt.pix.width = COLOR_BAR_WIDTH;
    fmt.fmt.pix.height = COLOR_BAR_HEIGHT;
    fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_YUYV;
    fmt.fmt.pix.colorspace = V4L2_COLORSPACE_DEFAULT;
    fmt.fmt.pix.xfer_func = V4L2_XFER_FUNC_DEFAULT;
    fmt.fmt.pix.ycbcr_enc = V4L2_YCBCR_ENC_DEFAULT;
    fmt.fmt.pix.quantization = V4L2_QUANTIZATION_DEFAULT;
    img_get_colorspace_data(&cam_rgb24, &fmt);
    colspace = cam_rgb24.colorspc;

    rgb24_buffer = load_raw_file(&exact_imgs[0], &cam_rgb24);

    // FIXME: add a command line arg to enable it
    assert_int_equal(test_save_png_named(exact_imgs[0].name, rgb24_buffer,
                                         cam_rgb24.width, cam_rgb24.height, 1), 0);

    return 0;
}

static int group_teardown(void **)
{
    free(rgb24_buffer);

    return 0;
}

static void add_tests(struct CMUnitTest **tests, unsigned *num_tests,
                      struct raw_imgs *imgs, unsigned num_imgs,
                      void test_func(void **), const char *name_fmt,
                      bool ignore_missing)
{
    struct CMUnitTest *el;
    char *test_name;
    unsigned i, added = 0, slot = *num_tests;
    int rc;

    for (i = 0; i < num_imgs; i++)
        added += !ignore_missing || imgs[i].fname != NULL;

    *tests = reallocarray(*tests, *num_tests + added, sizeof(*el));
    assert_non_null(*tests);

    for (i = 0; i < num_imgs; i++) {
        if (ignore_missing && !imgs[i].fname)
            continue;

        rc = asprintf(&test_name, name_fmt, imgs[i].name);
        assert_true(rc > 0);

        el = &((*tests)[slot++]);

        memset(el, 0, sizeof(*el));
        el->test_func = test_func;
        el->initial_state = &imgs[i];
        el->name = test_name;
        imgs[i].psnr = -1000.0;

    }

    *num_tests += added;
}

static void test_generate_fixture_case(void **state)
{
    struct raw_imgs *img = *state;
    int rc;

    if (!strcmp(img->name, "NV61") && !nv16_supported) {
        img->fname = NULL;
        skip();
    }

    if (!strcmp(img->name, "H264") && !test_ffmpeg_h264_encoder(false)) {
        img->fname = NULL;
        fprintf(stderr, "Skipping H264 fixture: FFmpeg has no supported software H.264 encoder\n");
        skip();
    }

    rc = color_bars_generate(img);
    if (rc && (!strcmp(img->name, "NV16") ||
               !strcmp(img->name, "NV61"))) {
        if (!strcmp(img->name, "NV16"))
            nv16_supported = false;
        free(img->fname);
        img->fname = NULL;
        fprintf(stderr, "Skipping %s fixture: FFmpeg lacks required format support\n",
                img->name);
        skip();
    }

    assert_int_equal(rc, 0);
}

int test_img_convert(void)
{
    static struct CMUnitTest *img_tests = NULL;
    static struct CMUnitTest *img_gen;
    unsigned num_img_tests = 0;
    unsigned num_img_gen = 0;
    unsigned i;
    int rc;

   /* Dynamically create the ffmeg generation tests */
    add_tests(&img_gen, &num_img_gen, exact_imgs, ARRAY_SIZE(exact_imgs),
              test_generate_fixture_case, "generate %s fixture", false);
    add_tests(&img_gen, &num_img_gen, aprox_imgs, ARRAY_SIZE(aprox_imgs),
              test_generate_fixture_case, "generate %s fixture", false);

    rc = _cmocka_run_group_tests("ffmpeg_generate", img_gen, num_img_gen,
                                 NULL, NULL);

   /* Dynamically create the img_tests */
    add_tests(&img_tests, &num_img_tests, &exact_imgs[1],
              ARRAY_SIZE(exact_imgs) - 1, test_lossless,
              "test %s to RGB", true);

    add_tests(&img_tests, &num_img_tests, aprox_imgs,
              ARRAY_SIZE(aprox_imgs), test_psnr,
              "test %s to RGB", true);

    rc = _cmocka_run_group_tests("img_convert", img_tests, num_img_tests,
                                 group_setup, group_teardown);

    printf("\nPeak S/N Ratio for lossy image reconstruct:\n");
    printf("+----------+-----------+\n");
    printf("| %-8s | %9s |\n", "Format", "PSNR (dB)");
    printf("+----------+-----------+\n");
    for (i = 0; i < ARRAY_SIZE(aprox_imgs); i++) {
        struct raw_imgs *img = &aprox_imgs[i];

        if (!img->fname)
            continue;
        if (img->psnr > -1000.0)
            printf("| %-8s | %9.2f |\n", img->name, img->psnr);
        else
            printf("| %-8s | %9s |\n", img->name, "--");
    }
    printf("+----------+-----------+\n");

    return rc;
}

REGISTER_TEST(test_img_convert, 0);
