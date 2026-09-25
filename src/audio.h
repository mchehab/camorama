#ifndef CAMORAMA_AUDIO_H
#define CAMORAMA_AUDIO_H

#include "camera-backend.h"

#include <config.h>

#ifdef HAVE_AUDIO

gboolean cam_audio_start(cam_t *cam);
void cam_audio_stop(cam_t *cam);
gboolean cam_audio_available(cam_t *cam);
void cam_audio_enable(GtkWidget *button, cam_t *cam);
void cam_audio_volume(GtkRange *range, cam_t *cam);

#else
static inline gboolean cam_audio_available(cam_t *) { return FALSE; }
static inline gboolean cam_audio_start(cam_t *) { return FALSE; }
static inline void cam_audio_stop(cam_t *) {}
static inline void cam_audio_enable(GtkWidget *, cam_t *) {}
static inline void cam_audio_volume(GtkRange *, cam_t *){}

#endif // HAVE_AUDIO

#endif
