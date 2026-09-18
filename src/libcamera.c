/* SPDX-License-Identifier: GPL-2.0-or-later */
#include <glib/gi18n.h>
#include <stdlib.h>
#include <string.h>
#include <config.h>

#include "camorama-libcamera.h"
#include "libcamera-bridge.h"
#include "support.h"

static void show_libcamera_error(const char *operation, char *detail)
{
    char *message;

    message = g_strdup_printf(_("libcamera: %s: %s"), operation,
                              detail ? detail : _("unknown error"));
    error_dialog(message);
    g_free(message);
    libcamera_bridge_free_string(detail);
}

gboolean libcamera_backend_available(void)
{
    return TRUE;
}

int libcamera_cam_open(cam_t *cam)
{
    libcamera_bridge_t *bridge;
    const char *camera_id;
    char *error = NULL;

    bridge = libcamera_bridge_create(cam->video_dev, cam->debug, &error);
    if (!bridge) {
        show_libcamera_error(_("could not open camera"), error);
        return -1;
    }

    cam->libcamera = bridge;
    camera_id = libcamera_bridge_camera_id(bridge);
    g_free(cam->video_dev);
    cam->video_dev = g_strdup(camera_id);
    return 0;
}

int libcamera_cam_close(cam_t *cam)
{
    if (cam->libcamera) {
        libcamera_bridge_destroy(cam->libcamera);
        cam->libcamera = NULL;
    }
    cam->dev = -1;
    return 0;
}

unsigned char *libcamera_cam_read(cam_t *cam)
{
    char *error = NULL;
    size_t size = (size_t)cam->width * cam->height * 3;

    if (libcamera_bridge_read(cam->libcamera, cam->pic_buf, size, 1000,
                              &error)) {
        if (cam->debug && error)
            g_warning("libcamera: %s", error);
        libcamera_bridge_free_string(error);
        return NULL;
    }

    return cam->pic_buf;
}

void libcamera_get_supported_resolutions(cam_t *cam)
{
    libcamera_bridge_t *bridge = cam->libcamera;
    unsigned int count, i;

    free(cam->res);
    cam->res = NULL;
    cam->n_res = 0;

    count = libcamera_bridge_num_sizes(bridge);
    if (!count)
        return;

    cam->res = calloc(count, sizeof(*cam->res));
    if (!cam->res)
        return;

    for (i = 0; i < count; i++) {
        struct resolutions *res = &cam->res[cam->n_res];

        if (libcamera_bridge_get_size(bridge, i, &res->x, &res->y))
            continue;
        res->pixformat = V4L2_PIX_FMT_RGB24;
        res->depth = 24;
        res->max_fps = -1;
        res->order = 0;
        cam->n_res++;
    }
}

int libcamera_camera_cap(cam_t *cam)
{
    const char *name;
    unsigned int i;

    cam->rdir_ok = FALSE;
    cam->min_width = (unsigned int)-1;
    cam->min_height = (unsigned int)-1;
    cam->max_width = 0;
    cam->max_height = 0;
    libcamera_get_supported_resolutions(cam);

    if (!cam->n_res) {
        show_libcamera_error(_("could not query camera formats"), NULL);
        return 1;
    }

    for (i = 0; i < cam->n_res; i++) {
        cam->min_width = MIN(cam->min_width, cam->res[i].x);
        cam->min_height = MIN(cam->min_height, cam->res[i].y);
        cam->max_width = MAX(cam->max_width, cam->res[i].x);
        cam->max_height = MAX(cam->max_height, cam->res[i].y);
    }

    if (!cam->width || !cam->height) {
        if (cam->size == PICMAX) {
            cam->width = cam->max_width;
            cam->height = cam->max_height;
        } else if (cam->size == PICMIN) {
            cam->width = cam->min_width;
            cam->height = cam->min_height;
        } else {
            cam->width = cam->max_width / 2;
            cam->height = cam->max_height / 2;
        }
    }
    libcamera_try_set_win_info(cam, &cam->width, &cam->height);

    name = libcamera_bridge_camera_name(cam->libcamera);
    g_strlcpy(cam->name, name ? name : cam->video_dev, sizeof(cam->name));
    cam->read = FALSE;
    cam->userptr = FALSE;
    cam->use_libv4l = FALSE;
    return 0;
}

void libcamera_try_set_win_info(cam_t *cam, unsigned int *width,
                                unsigned int *height)
{
    libcamera_bridge_try_size(cam->libcamera, width, height);
}

void libcamera_set_win_info(cam_t *cam)
{
    char *error = NULL;
    unsigned int stride = 0;

    if (libcamera_bridge_configure(cam->libcamera, &cam->width, &cam->height,
                                   &stride, &error)) {
        show_libcamera_error(_("could not configure camera"), error);
        exit(EXIT_FAILURE);
    }

    cam->pixformat = V4L2_PIX_FMT_RGB24;
    cam->bpp = 24;
    cam->bytesperline = stride;
    cam->sizeimage = stride * cam->height;
}

void libcamera_get_win_info(cam_t *cam)
{
    (void)cam;
}

void libcamera_get_pic_info(cam_t *cam)
{
    cam_free_controls(cam);
    cam->contrast = -1;
    cam->brightness = -1;
    cam->whiteness = -1;
    cam->colour = -1;
    cam->hue = -1;
    cam->zoom = -1;
    cam->zoom_cid = 0;
}

void libcamera_start_streaming(cam_t *cam)
{
    char *error = NULL;

    if (libcamera_bridge_start(cam->libcamera, &error)) {
        show_libcamera_error(_("could not start camera"), error);
        exit(EXIT_FAILURE);
    }
}

void libcamera_stop_streaming(cam_t *cam)
{
    if (cam->libcamera)
        libcamera_bridge_stop(cam->libcamera);
}
