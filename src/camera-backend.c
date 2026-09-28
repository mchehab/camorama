/* SPDX-License-Identifier: GPL-2.0-or-later */
#include <config.h>
#include <errno.h>

#include "audio.h"
#include "camera-backend.h"
#include "img_ffmpeg.h"
#include "v4l.h"
#include "streaming.h"
#include "support.h"

static const struct camera_backend *camera_backend_get(const cam_t *cam)
{
    return cam->backend ? cam->backend : &v4l_camera_backend;
}

video_controls_t *cam_find_control_per_id(cam_t *cam, guint32 id)
{
    video_controls_t *control = cam->controls;

    while (control) {
        if (control->id == id)
            return control;
        control = control->next;
    }
    return NULL;
}

void cam_free_controls(cam_t *cam)
{
    video_controls_t *control = cam->controls;

    while (control) {
        video_controls_t *next = control->next;
        guint i;

        g_free(control->name);
        g_free(control->group);
        for (i = 0; i < control->menu_size; i++)
            g_free(control->menu[i].name);
        g_free(control->menu);
        g_free(control);
        control = next;
    }
    cam->controls = NULL;
}

void camera_backend_set(cam_t *cam, const struct camera_backend *backend)
{
    cam->backend = backend;
}

void camera_backend_select(cam_t *cam, const struct camera_backend *backend)
{
    camera_backend_set(cam, backend);
}

gboolean camera_backend_is_libcamera(const cam_t *cam)
{
    return camera_backend_get(cam)->is_libcamera;
}

const char *camera_backend_name(const cam_t *cam)
{
    return camera_backend_get(cam)->name;
}

int cam_open(cam_t *cam, int oflag)
{
    const struct camera_backend *backend = camera_backend_get(cam);

    return backend->open ? backend->open(cam, oflag) : -ENOTSUP;
}

int cam_close(cam_t *cam)
{
    const struct camera_backend *backend = camera_backend_get(cam);

    return backend->close ? backend->close(cam) : -ENOTSUP;
}

unsigned char *cam_read(cam_t *cam, unsigned char *output)
{
    const struct camera_backend *backend = camera_backend_get(cam);

    return backend->read ? backend->read(cam, output) : NULL;
}

int cam_cancel_read(cam_t *cam)
{
    const struct camera_backend *backend = camera_backend_get(cam);

    if (!backend->cancel_read)
        return -ENOTSUP;
    backend->cancel_read(cam);
    return 0;
}

int cam_query_controls(cam_t *cam)
{
    const struct camera_backend *backend = camera_backend_get(cam);

    return backend->query_controls ? backend->query_controls(cam) : -ENOTSUP;
}

int cam_set_control(cam_t *cam, guint32 id, void *value)
{
    const struct camera_backend *backend = camera_backend_get(cam);

    return backend->set_control ? backend->set_control(cam, id, value) :
                                  -ENOTSUP;
}

int cam_get_control(cam_t *cam, guint32 id, void *value)
{
    const struct camera_backend *backend = camera_backend_get(cam);

    return backend->get_control ? backend->get_control(cam, id, value) :
                                  -ENOTSUP;
}

GArray *cam_get_frame_intervals(cam_t *cam)
{
    const struct camera_backend *backend = camera_backend_get(cam);

    return backend->get_frame_intervals ?
           backend->get_frame_intervals(cam) : NULL;
}

gboolean cam_set_frame_interval(cam_t *cam,
                                const struct v4l2_fract *interval)
{
    const struct camera_backend *backend = camera_backend_get(cam);

    return backend->set_frame_interval &&
           backend->set_frame_interval(cam, interval);
}

gboolean cam_get_frame_interval(cam_t *cam, struct v4l2_fract *interval)
{
    const struct camera_backend *backend = camera_backend_get(cam);

    return backend->get_frame_interval &&
           backend->get_frame_interval(cam, interval);
}

int camera_cap(cam_t *cam)
{
    const struct camera_backend *backend = camera_backend_get(cam);

    return backend->camera_cap ? backend->camera_cap(cam) : -ENOTSUP;
}

int get_pic_info(cam_t *cam)
{
    const struct camera_backend *backend = camera_backend_get(cam);

    if (!backend->get_pic_info)
        return -ENOTSUP;
    backend->get_pic_info(cam);
    return 0;
}

int get_win_info(cam_t *cam)
{
    const struct camera_backend *backend = camera_backend_get(cam);

    if (!backend->get_win_info)
        return -ENOTSUP;
    backend->get_win_info(cam);
    return 0;
}

int try_set_win_info(cam_t *cam, unsigned int pixformat,
                     unsigned int *width, unsigned int *height)
{
    const struct camera_backend *backend = camera_backend_get(cam);

    if (!backend->try_set_win_info)
        return -ENOTSUP;
    backend->try_set_win_info(cam, pixformat, width, height);
    return 0;
}

int set_win_info(cam_t *cam)
{
    const struct camera_backend *backend = camera_backend_get(cam);

    if (!backend->set_win_info)
        return -ENOTSUP;
    backend->set_win_info(cam);
    return 0;
}

int get_supported_resolutions(cam_t *cam, gboolean all_supported)
{
    const struct camera_backend *backend = camera_backend_get(cam);

    if (!backend->get_supported_resolutions)
        return -ENOTSUP;
    backend->get_supported_resolutions(cam, all_supported);
    return 0;
}

int print_cam(cam_t *cam)
{
    const struct camera_backend *backend = camera_backend_get(cam);

    if (!backend->print_cam)
        return -ENOTSUP;
    backend->print_cam(cam);
    return 0;
}

int start_streaming(cam_t *cam)
{
    const struct camera_backend *backend = camera_backend_get(cam);
    struct v4l2_fract interval;
    char *stream_method = "";
    float fps;

    if (cam->read && !cam->can_read) {
        g_warning("Device doesn't support read()");
        cam->read = FALSE;
    }

    if (cam->read) {
        stream_method = "read";
    } else {
        if (backend->start_streaming)
            backend->start_streaming(cam);
        else
            return -ENOTSUP;

        if (cam->userptr)
            stream_method = "userptr";
        else
            stream_method = "mmap";
    }

    if (cam->debug == TRUE) {
        if (cam_get_frame_interval(cam, &interval)) {
            fps = ((float)interval.denominator)/interval.numerator;

            printf("Start streaming (%s) with FOURCC: '%c%c%c%c' (%dx%d %.2f fps)\n",
                   stream_method,
                   cam->pixformat & 0xff,
                   (cam->pixformat >> 8) & 0xff,
                   (cam->pixformat >> 16) & 0xff,
                   cam->pixformat >> 24,
                   cam->width, cam->height,
                   fps);
    } else {
            printf("Start streaming (%s) with FOURCC: '%c%c%c%c'(%dx%d) \n",
                   stream_method,
                   cam->pixformat & 0xff,
                   (cam->pixformat >> 8) & 0xff,
                   (cam->pixformat >> 16) & 0xff,
                   cam->pixformat >> 24,
                   cam->width, cam->height);
       }
    }

    if (cam->audio_enabled && cam->audio_available && !cam_audio_start(cam))
        g_warning("Could not restart audio bridge");

    if (cam_stream_configure(cam))
        cam_stream_start(cam);

    return 0;
}

int stop_streaming(cam_t *cam)
{
    const struct camera_backend *backend = camera_backend_get(cam);

    cam_audio_stop(cam);
    cam_stream_stop(cam);

    if (!cam->read && backend->stop_streaming)
        backend->stop_streaming(cam);

    img_ffmpeg_free_converter(&cam->converter);

    return 0;
}

int cam_set_max_fps(cam_t *cam)
{
    GArray *intervals = cam_get_frame_intervals(cam);
    const struct v4l2_fract *best = NULL;
    guint i;

    if (!intervals)
        return -ENOTSUP;
    for (i = 0; i < intervals->len; i++) {
        const struct v4l2_fract *interval = &g_array_index(
            intervals, struct v4l2_fract, i);

        if (!best || (guint64)interval->numerator * best->denominator <
                     (guint64)best->numerator * interval->denominator)
            best = interval;
    }
    if (best && !cam_set_frame_interval(cam, best)) {
        g_array_unref(intervals);
        return -ENOTSUP;
    }
    g_array_unref(intervals);
    return 0;
}
