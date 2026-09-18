/* SPDX-License-Identifier: GPL-2.0-or-later */
#ifndef CAMORAMA_CAMERA_BACKEND_H
#define CAMORAMA_CAMERA_BACKEND_H

#include "v4l.h"

typedef struct camera_backend {
    const char *name;
    int (*open)(cam_t *cam, int oflag);
    int (*close)(cam_t *cam);
    unsigned char *(*read)(cam_t *cam);
    int (*query_controls)(cam_t *cam);
    int (*set_control)(cam_t *cam, guint32 id, void *value);
    int (*get_control)(cam_t *cam, guint32 id, void *value);
    int (*camera_cap)(cam_t *cam);
    void (*get_pic_info)(cam_t *cam);
    void (*get_win_info)(cam_t *cam);
    void (*try_set_win_info)(cam_t *cam, unsigned int pixformat,
                             unsigned int *width, unsigned int *height);
    void (*set_win_info)(cam_t *cam);
    void (*get_supported_resolutions)(cam_t *cam, gboolean all_supported);
    void (*start_streaming)(cam_t *cam);
    void (*stop_streaming)(cam_t *cam);
} camera_backend_t;

extern const camera_backend_t v4l_camera_backend;

gboolean camera_backend_select(cam_t *cam, gboolean use_libcamera);
gboolean camera_backend_is_libcamera(const cam_t *cam);
const char *camera_backend_name(const cam_t *cam);

#endif
