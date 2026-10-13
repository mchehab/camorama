/* SPDX-License-Identifier: GPL-2.0-or-later */
#ifndef CAMORAMA_LIBCAMERA_H
#define CAMORAMA_LIBCAMERA_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct libcamera_bridge libcamera_bridge_t;

struct libcamera_camera_info {
	char *id;
	char *name;
};

int libcamera_bridge_list_cameras(struct libcamera_camera_info **cameras,
					  unsigned int *count);
void libcamera_bridge_free_cameras(struct libcamera_camera_info *cameras,
					   unsigned int count);

#ifdef __cplusplus
}
#endif

#endif
