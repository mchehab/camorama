#ifndef CAMORAMA_STREAMING_H
#define CAMORAMA_STREAMING_H

#include "camera-backend.h"

gboolean cam_stream_configure(cam_t *cam);
void cam_stream_start(cam_t *cam);
void cam_stream_stop(cam_t *cam);
void cam_stream_cleanup(cam_t *cam);

#endif
