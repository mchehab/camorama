/* SPDX-License-Identifier: GPL-2.0-or-later */
#ifndef CAMORAMA_LIBCAMERA_H
#define CAMORAMA_LIBCAMERA_H

#include "v4l.h"

#ifdef HAVE_LIBCAMERA
gboolean libcamera_backend_available(void);
int libcamera_cam_open(cam_t *cam);
int libcamera_cam_close(cam_t *cam);
unsigned char *libcamera_cam_read(cam_t *cam);
int libcamera_camera_cap(cam_t *cam);
void libcamera_get_supported_resolutions(cam_t *cam);
void libcamera_try_set_win_info(cam_t *cam, unsigned int *width,
                                unsigned int *height);
void libcamera_set_win_info(cam_t *cam);
void libcamera_get_win_info(cam_t *cam);
void libcamera_get_pic_info(cam_t *cam);
void libcamera_start_streaming(cam_t *cam);
void libcamera_stop_streaming(cam_t *cam);
#else
static inline gboolean libcamera_backend_available(void)
{
    return FALSE;
}

static inline int libcamera_cam_open(cam_t *cam)
{
    (void)cam;
    return -1;
}

static inline int libcamera_cam_close(cam_t *cam)
{
    cam->dev = -1;
    return 0;
}

static inline unsigned char *libcamera_cam_read(cam_t *cam)
{
    (void)cam;
    return NULL;
}

static inline int libcamera_camera_cap(cam_t *cam)
{
    (void)cam;
    return 1;
}

static inline void libcamera_get_supported_resolutions(cam_t *cam)
{
    (void)cam;
}

static inline void libcamera_try_set_win_info(cam_t *cam,
                                               unsigned int *width,
                                               unsigned int *height)
{
    (void)cam;
    (void)width;
    (void)height;
}

static inline void libcamera_set_win_info(cam_t *cam)
{
    (void)cam;
}

static inline void libcamera_get_win_info(cam_t *cam)
{
    (void)cam;
}

static inline void libcamera_get_pic_info(cam_t *cam)
{
    (void)cam;
}

static inline void libcamera_start_streaming(cam_t *cam)
{
    (void)cam;
}

static inline void libcamera_stop_streaming(cam_t *cam)
{
    (void)cam;
}
#endif

#endif
