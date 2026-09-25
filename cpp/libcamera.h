/* SPDX-License-Identifier: GPL-2.0-or-later */
#ifndef CAMORAMA_LIBCAMERA_H
#define CAMORAMA_LIBCAMERA_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct libcamera_bridge libcamera_bridge_t;

enum libcamera_control_type {
	LIBCAMERA_CONTROL_BOOL,
	LIBCAMERA_CONTROL_INTEGER,
	LIBCAMERA_CONTROL_INTEGER64,
	LIBCAMERA_CONTROL_FLOAT,
	LIBCAMERA_CONTROL_MENU,
};

struct libcamera_camera_info {
	char *id;
	char *name;
};

int libcamera_bridge_list_cameras(struct libcamera_camera_info **cameras,
					  unsigned int *count, char **error);
void libcamera_bridge_free_cameras(struct libcamera_camera_info *cameras,
					   unsigned int count);

libcamera_bridge_t *libcamera_bridge_create(const char *camera_id, int debug,
                                             char **error);
void libcamera_bridge_destroy(libcamera_bridge_t *bridge);
const char *libcamera_bridge_camera_id(const libcamera_bridge_t *bridge);
const char *libcamera_bridge_camera_name(const libcamera_bridge_t *bridge);
unsigned int libcamera_bridge_pixel_format(const libcamera_bridge_t *bridge);
unsigned int libcamera_bridge_num_sizes(const libcamera_bridge_t *bridge);
int libcamera_bridge_get_size(const libcamera_bridge_t *bridge,
                              unsigned int index, unsigned int *width,
                              unsigned int *height,
                              unsigned int *pixformat);
void libcamera_bridge_try_size(const libcamera_bridge_t *bridge,
                               unsigned int *width, unsigned int *height);
int libcamera_bridge_configure(libcamera_bridge_t *bridge,
                               unsigned int *width, unsigned int *height,
                               unsigned int *stride,
                               unsigned int *frame_size,
                               unsigned int *pixformat, char **error);
int libcamera_bridge_start(libcamera_bridge_t *bridge, char **error);
void libcamera_bridge_stop(libcamera_bridge_t *bridge);
int libcamera_bridge_set_frame_duration(libcamera_bridge_t *bridge,
                                        uint64_t duration_us);
uint64_t libcamera_bridge_frame_duration(const libcamera_bridge_t *bridge);
int libcamera_bridge_supports_frame_duration(const libcamera_bridge_t *bridge);
unsigned int libcamera_bridge_num_controls(const libcamera_bridge_t *bridge);
int libcamera_bridge_get_control_info(const libcamera_bridge_t *bridge,
                                      unsigned int index, uint32_t *id,
                                      const char **name, int *type,
                                      int32_t *min, int32_t *max,
                                      int32_t *def, int32_t *step);
int libcamera_bridge_set_control(libcamera_bridge_t *bridge, uint32_t id,
                                 int32_t value);
int libcamera_bridge_get_control(const libcamera_bridge_t *bridge, uint32_t id,
                                 int32_t *value);
int libcamera_bridge_read(libcamera_bridge_t *bridge, unsigned char *output,
                          size_t output_size, unsigned int timeout_ms,
                          size_t *bytes_used, char **error);
void libcamera_bridge_free_string(char *string);

#ifdef __cplusplus
}
#endif

#endif
