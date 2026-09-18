/* SPDX-License-Identifier: GPL-2.0-or-later */
#include <config.h>

#include "camera-backend.h"
#include "img_convert.h"

#ifdef HAVE_LIBCAMERA
extern const camera_backend_t libcamera_camera_backend;
#endif

gboolean camera_backend_select(cam_t *cam, gboolean use_libcamera)
{
    if (!use_libcamera) {
        cam->backend = &v4l_camera_backend;
        return TRUE;
    }

#ifdef HAVE_LIBCAMERA
    cam->backend = &libcamera_camera_backend;
    return TRUE;
#else
    return FALSE;
#endif
}

gboolean camera_backend_is_libcamera(const cam_t *cam)
{
#ifdef HAVE_LIBCAMERA
    return cam->backend == &libcamera_camera_backend;
#else
    (void)cam;
    return FALSE;
#endif
}

const char *camera_backend_name(const cam_t *cam)
{
    return cam->backend->name;
}

int cam_open(cam_t *cam, int oflag)
{
    return cam->backend->open(cam, oflag);
}

int cam_close(cam_t *cam)
{
    int ret = cam->backend->close(cam);

    img_converter_free(cam->converter);
    cam->converter = NULL;
    return ret;
}

unsigned char *cam_read(cam_t *cam)
{
    return cam->backend->read(cam);
}

int cam_query_controls(cam_t *cam)
{
    return cam->backend->query_controls(cam);
}

int cam_set_control(cam_t *cam, guint32 id, void *value)
{
    return cam->backend->set_control(cam, id, value);
}

int cam_get_control(cam_t *cam, guint32 id, void *value)
{
    return cam->backend->get_control(cam, id, value);
}

int camera_cap(cam_t *cam)
{
    return cam->backend->camera_cap(cam);
}

void get_pic_info(cam_t *cam)
{
    cam->backend->get_pic_info(cam);
}

void get_win_info(cam_t *cam)
{
    cam->backend->get_win_info(cam);
}

void try_set_win_info(cam_t *cam, unsigned int pixformat,
                      unsigned int *width, unsigned int *height)
{
    cam->backend->try_set_win_info(cam, pixformat, width, height);
}

void set_win_info(cam_t *cam)
{
    cam->backend->set_win_info(cam);
}

void get_supported_resolutions(cam_t *cam, gboolean all_supported)
{
    cam->backend->get_supported_resolutions(cam, all_supported);
}

void start_streaming(cam_t *cam)
{
    cam->backend->start_streaming(cam);
}

void stop_streaming(cam_t *cam)
{
    cam->backend->stop_streaming(cam);
}
