// SPDX-License-Identifier: GPL-2.0-or-later

#include <stddef.h>
#include <stdarg.h>
#include <setjmp.h>
#include <cmocka.h>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>

#include "unittest.h"
#include "src/streaming.h"

static gint captured_frames;

static void discard_expected_warning(const gchar *,
                                    GLogLevelFlags,
                                    const gchar *,
                                    gpointer)
{
}

static int fake_read(cam_t *cam, void *buffer, size_t size)
{
    unsigned char value = (unsigned char)g_atomic_int_get(&captured_frames);
    memset(buffer, value, size);
    g_atomic_int_inc(&captured_frames);
    /* Deterministic pacing makes the test independent of CPU speed. */
    g_usleep(1000);
    (void)cam;
    return 0;
}

static const struct cam_v4l_ops fake_ops = {
    .read = fake_read,
};

static void init_stream_cam(cam_t *cam, gboolean read_mode)
{
    cam->read = read_mode;
    cam->width = 4;
    cam->height = 2;
    cam->bpp = 24;
    cam->bytesperline = 12;
    cam->sizeimage = 24;
    cam->pixformat = V4L2_PIX_FMT_RGB24;
    cam->capture_input = g_malloc0(24);
    cam->stream_wakeup[0] = cam->stream_wakeup[1] = -1;
    g_mutex_init(&cam->display_mutex);
}

static void destroy_stream_cam(cam_t *cam)
{
    cam_stream_cleanup(cam);
    g_clear_object(&cam->pb);
    g_free(cam->capture_input);
    g_mutex_clear(&cam->display_mutex);
}

static void test_slow_ui_does_not_stop_capture(void **state)
{
    cam_t cam = { 0 };
    gint initial;
    (void)state;

    init_stream_cam(&cam, TRUE);
    g_atomic_int_set(&captured_frames, 0);
    cam_set_v4l_ops(&fake_ops);
    assert_true(cam_stream_configure(&cam));
    cam_stream_start(&cam);

    /* Leave the default main context unserviced to model slow UX/filtering. */
    g_usleep(40000);
    initial = g_atomic_int_get(&captured_frames);
    cam_stream_stop(&cam);
    cam_set_v4l_ops(NULL);

    assert_true(initial >= 10);
    assert_true(cam.stream_sequence >= 10);
    assert_true(cam.display_buffers[0].capacity >= 24);
    assert_true(cam.display_buffers[1].capacity >= 24);
    assert_null(cam.pb);

    /* Reconfiguration grows exchange storage and invalidates stale output. */
    cam.width = 8;
    cam.height = 4;
    cam.bytesperline = 24;
    cam.sizeimage = 96;
    cam.capture_input = g_realloc(cam.capture_input, 96);
    assert_true(cam_stream_configure(&cam));
    assert_true(cam.display_buffers[0].capacity >= 96);
    assert_true(cam.display_buffers[1].capacity >= 96);
    assert_int_equal(cam.display_state, CAM_DISPLAY_EMPTY);
    cam.width = G_MAXUINT;
    {
        guint log_handler = g_log_set_handler(NULL, G_LOG_LEVEL_WARNING,
                                              discard_expected_warning, NULL);

        assert_false(cam_stream_configure(&cam));
        g_log_remove_handler(NULL, log_handler);
    }
    cam.width = 8;

    for (int cycle = 0; cycle < 3; cycle++) {
        g_atomic_int_set(&captured_frames, 0);
        cam_set_v4l_ops(&fake_ops);
        cam_stream_start(&cam);
        g_usleep(10000);
        cam_stream_stop(&cam);
        cam_set_v4l_ops(NULL);
        assert_true(cam.stream_sequence >= (guint64)(10 * (cycle + 2)));
    }
    destroy_stream_cam(&cam);
}

static void test_stop_interrupts_capture_wait(void **state)
{
    cam_t cam = { 0 };
    int camera_pipe[2];
    int wakeup_pipe[2];
    gint64 started;
    gint64 elapsed;
    (void)state;

    assert_int_equal(pipe(camera_pipe), 0);
    assert_int_equal(pipe(wakeup_pipe), 0);
    assert_int_equal(fcntl(wakeup_pipe[0], F_SETFL, O_NONBLOCK), 0);
    init_stream_cam(&cam, FALSE);
    cam.dev = camera_pipe[0];
    cam.stream_wakeup[0] = wakeup_pipe[0];
    cam.stream_wakeup[1] = wakeup_pipe[1];
    assert_true(cam_stream_configure(&cam));
    cam_stream_start(&cam);
    g_usleep(20000);

    started = g_get_monotonic_time();
    cam_stream_stop(&cam);
    elapsed = g_get_monotonic_time() - started;
    assert_true(elapsed < G_USEC_PER_SEC);

    close(camera_pipe[0]);
    close(camera_pipe[1]);
    close(wakeup_pipe[0]);
    close(wakeup_pipe[1]);
    cam.stream_wakeup[0] = cam.stream_wakeup[1] = -1;
    destroy_stream_cam(&cam);
}

static int test_streaming_lifecycle(void)
{
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(test_slow_ui_does_not_stop_capture),
        cmocka_unit_test(test_stop_interrupts_capture_wait),
    };
    return _cmocka_run_group_tests("streaming_lifecycle", tests,
                                   G_N_ELEMENTS(tests), NULL, NULL);
}

REGISTER_TEST(test_streaming_lifecycle, 0);
