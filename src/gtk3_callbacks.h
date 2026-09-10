#ifndef CAMORAMA_GTK3_CALLBACKS_H
#define CAMORAMA_GTK3_CALLBACKS_H

#include "gtk_common_callbacks.h"

G_BEGIN_DECLS

gboolean on_window_state_event(GtkWidget *widget,
                               GdkEventWindowState *event, cam_t *cam);
gboolean gtk3_window_is_fullscreen(GtkWindow *window);
gboolean gtk3_draw_frame(GtkWidget *, cairo_t *, gpointer);
gboolean on_drawingarea_expose_event(GtkWidget *, GdkEventExpose *, cam_t *);

G_END_DECLS

#endif                          /* !CAMORAMA_GTK3_CALLBACKS_H */
