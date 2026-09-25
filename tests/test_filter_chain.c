// SPDX-License-Identifier: GPL-2.0-or-later

#include <stddef.h>
#include <stdarg.h>
#include <setjmp.h>
#include <cmocka.h>
#include <string.h>

#include "unittest.h"
#include "src/streaming.h"
#include "src/camorama-filter-chain.h"
#include "src/filter.h"

struct filter_stress_data {
    CamoramaFilterChain *chain;
    gint mutations;
};

static gint captured_frames;

static int fake_read(cam_t *cam, void *buffer, size_t size)
{
    gint frame = g_atomic_int_add(&captured_frames, 1);

    (void)cam;
    memset(buffer, (guchar)frame, size);
    return 0;
}

static const struct cam_v4l_ops fake_ops = {
    .read = fake_read,
};

static gpointer mutate_filter_chain(gpointer user_data)
{
    struct filter_stress_data *stress = user_data;
    const GType types[] = {
        camorama_filter_invert_get_type(),
        camorama_filter_mirror_get_type(),
    };
    guint count = 0;
    guint random = 0x6d2b79f5;
    guint i;

    for (i = 0; i < 5000; i++) {
        random ^= random << 13;
        random ^= random >> 17;
        random ^= random << 5;
        if (!count || ((random & 3) != 0 && count < 12)) {
            guint position = random % (count + 1);
            GType type = types[(random >> 8) & 1];
            CamoramaFilter *filter = g_object_new(type, NULL);

            camorama_filter_chain_insert(stress->chain, position, filter);
            count++;
        } else {
            camorama_filter_chain_remove(stress->chain, random % count);
            count--;
        }
        g_atomic_int_inc(&stress->mutations);
    }
    return NULL;
}

static void test_filter_chain_edits_during_streaming(void **state)
{
    cam_t cam = { 0 };
    struct filter_stress_data stress = { 0 };
    GThread *mutator;
    gint64 deadline;
    (void)state;

    cam.read = TRUE;
    cam.width = 320;
    cam.height = 240;
    cam.bpp = 24;
    cam.bytesperline = cam.width * 3;
    cam.sizeimage = cam.bytesperline * cam.height;
    cam.pixformat = V4L2_PIX_FMT_RGB24;
    cam.capture_input = g_malloc0(cam.sizeimage);
    cam.stream_wakeup[0] = cam.stream_wakeup[1] = -1;
    g_mutex_init(&cam.display_mutex);
    camorama_filters_init();
    cam.filter_chain = camorama_filter_chain_new();
    camorama_filter_chain_set_data(cam.filter_chain, &cam);
    stress.chain = cam.filter_chain;

    g_atomic_int_set(&captured_frames, 0);
    cam_set_v4l_ops(&fake_ops);
    assert_true(cam_stream_configure(&cam));
    cam_stream_start(&cam);

    /* Don't let the mutator finish before the capture thread gets scheduled. */
    deadline = g_get_monotonic_time() + 2 * G_TIME_SPAN_SECOND;
    while (g_atomic_int_get(&captured_frames) < 3 &&
           g_get_monotonic_time() < deadline) {
        while (g_main_context_iteration(NULL, FALSE))
            ;
        g_usleep(1000);
    }
    assert_true(g_atomic_int_get(&captured_frames) >= 3);

    mutator = g_thread_new("filter-chain-stress", mutate_filter_chain,
                           &stress);

    deadline = g_get_monotonic_time() + 2 * G_TIME_SPAN_SECOND;
    while (g_atomic_int_get(&stress.mutations) < 5000 &&
           g_get_monotonic_time() < deadline) {
        while (g_main_context_iteration(NULL, FALSE))
            ;
        g_usleep(250);
    }
    g_thread_join(mutator);
    while (g_main_context_iteration(NULL, FALSE))
        ;

    cam_stream_stop(&cam);
    cam_set_v4l_ops(NULL);
    assert_int_equal(g_atomic_int_get(&stress.mutations), 5000);
    assert_true(g_atomic_int_get(&captured_frames) > 2);
    assert_non_null(cam.pb);

    g_clear_object(&cam.filter_chain);
    cam_stream_cleanup(&cam);
    g_clear_object(&cam.pb);
    g_free(cam.capture_input);
    g_mutex_clear(&cam.display_mutex);
}

static int test_filter_chain(void)
{
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(test_filter_chain_edits_during_streaming),
    };

    return _cmocka_run_group_tests("filter_chain_concurrency", tests,
                                   G_N_ELEMENTS(tests), NULL, NULL);
}

REGISTER_TEST(test_filter_chain, 0);
