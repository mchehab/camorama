#ifndef CAMORAMA_GTK4_CALLBACKS_H
#define CAMORAMA_GTK4_CALLBACKS_H

#include "gtk_common_callbacks.h"

G_BEGIN_DECLS

void gtk4_drawing_area_resize(GtkDrawingArea *, int, int, cam_t *cam);
void on_window_fullscreen_changed(GtkWindow *window, GParamSpec *pspec,
                                  cam_t *cam);
void gtk4_draw_frame(GtkDrawingArea *, cairo_t *, int, int, gpointer);

G_END_DECLS

#endif                          /* !CAMORAMA_GTK4_CALLBACKS_H */
