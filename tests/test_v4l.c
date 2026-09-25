// SPDX-License-Identifier: GPL-2.0-or-later

#include <stddef.h>
#include <stdarg.h>
#include <errno.h>
#include <setjmp.h>
#include <cmocka.h>

#include "unittest.h"
#include "src/camera-backend.h"
#include "src/img_convert.h"
#include "mock_v4l.h"

/* Globals normally owned by main.c; the test links the camera library alone. */
int frame_number;
GtkWidget *prefswindow, *dentry, *entry2, *string_entry, *format_selection;
GtkWidget *host_entry, *protocol, *rdir_entry, *filename_entry;
int frames, frames2, seconds;


static int find_resolution(const cam_t *cam, unsigned int pixformat,
                           unsigned int width, unsigned int height,
                           float fps)
{
    unsigned int i;
    for (i = 0; i < cam->n_res; i++)
        if (cam->res[i].pixformat == pixformat &&
            cam->res[i].x == width && cam->res[i].y == height &&
            cam->res[i].max_fps == fps)
            return 1;
    return 0;
}

static void assert_sizes_present(const cam_t *cam, unsigned int pixformat,
                                 const struct mock_v4l_size *sizes,
                                 unsigned int count, int supported)
{
    unsigned int i;
    for (i = 0; i < count; i++) {
        assert_int_equal(find_resolution(cam, pixformat, sizes[i].width,
                                         sizes[i].height, sizes[i].max_fps),
                         supported);
    }
}

static void test_c920_supported_resolutions(void **state)
{
    cam_t cam = { 0 };
    unsigned int expected = mock_c920_yuyv_sizes_count;
    unsigned int i;
    int have_ffmpeg = img_format_get(V4L2_PIX_FMT_MJPEG) != NULL;
    (void)state;

    cam.dev = -1;
    cam.min_width = cam.min_height = (unsigned int)-1;
    cam_set_v4l_ops(&mock_c920_v4l_ops);
    get_supported_resolutions(&cam, TRUE);
    cam_set_v4l_ops(NULL);

    if (have_ffmpeg)
        expected = mock_c920_compressed_sizes_count + 2;
    assert_int_equal(cam.n_res, expected);
    if (have_ffmpeg) {
        for (i = 0; i < mock_c920_compressed_sizes_count; i++)
            assert_true(find_resolution(&cam, V4L2_PIX_FMT_MJPEG,
                                        mock_c920_compressed_sizes[i].width,
                                        mock_c920_compressed_sizes[i].height,
                                        30));
        assert_true(find_resolution(&cam, V4L2_PIX_FMT_YUYV, 2304, 1296, 2));
        assert_true(find_resolution(&cam, V4L2_PIX_FMT_YUYV, 2304, 1536, 2));
        assert_false(find_resolution(&cam, V4L2_PIX_FMT_H264, 640, 480, 30));
    } else {
        assert_sizes_present(&cam, V4L2_PIX_FMT_YUYV, mock_c920_yuyv_sizes,
                             mock_c920_yuyv_sizes_count, 1);
    }
    free(cam.res);
}

static void test_c920_read_backend(void **state)
{
    cam_t cam = { 0 };
    size_t frame_size = 160 * 90 * 3;
    unsigned char *first;
    unsigned char *display_data;
    unsigned int i;
    int different = 0;
    (void)state;

    cam.video_dev = (char *)"mock-c920";
    cam.read = TRUE;
    cam.pixformat = V4L2_PIX_FMT_YUYV;
    cam.width = 160;
    cam.height = 90;
    cam.bpp = 16;
    cam.capture_input = calloc(160 * 90, 2);
    display_data = malloc(frame_size);
    assert_non_null(cam.capture_input);
    assert_non_null(display_data);

    cam_set_v4l_ops(&mock_c920_v4l_ops);
    cam.dev = cam_open(&cam, O_RDONLY);
    assert_int_equal(cam.dev, 73);
    assert_non_null(cam_read(&cam, display_data));
    first = malloc(frame_size);
    assert_non_null(first);
    memcpy(first, display_data, frame_size);
    assert_non_null(cam_read(&cam, display_data));
    for (i = 0; i < frame_size; i++)
        different |= first[i] != display_data[i];
    assert_true(different);
    assert_int_equal(cam_close(&cam), 0);
    cam_set_v4l_ops(NULL);

    free(first);
    free(display_data);
    free(cam.capture_input);
}

static void test_eye_supported_resolutions(void **state)
{
    cam_t cam = { 0 };
    (void)state;

    cam.dev = -1;
    cam.min_width = cam.min_height = (unsigned int)-1;
    cam_set_v4l_ops(&mock_eye_v4l_ops);
    get_supported_resolutions(&cam, TRUE);
    cam_set_v4l_ops(NULL);

    assert_int_equal(cam.n_res, mock_eye_sizes_count);
    assert_true(find_resolution(&cam, V4L2_PIX_FMT_YUYV, 320, 240, 187));
    assert_true(find_resolution(&cam, V4L2_PIX_FMT_YUYV, 640, 480, 60));
    assert_false(find_resolution(&cam, V4L2_PIX_FMT_SGRBG8, 320, 240, 187));
    assert_false(find_resolution(&cam, V4L2_PIX_FMT_SGRBG8, 640, 480, 60));
    free(cam.res);
}

static int test_v4l_resolutions(void)
{
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(test_eye_supported_resolutions),
        cmocka_unit_test(test_c920_supported_resolutions),
        cmocka_unit_test(test_c920_read_backend),
    };
    return _cmocka_run_group_tests("v4l_resolutions", tests,
                                   sizeof(tests) / sizeof(tests[0]), NULL, NULL);
}

REGISTER_TEST(test_v4l_resolutions, 0);
