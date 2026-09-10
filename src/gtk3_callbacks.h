#ifndef CAMORAMA_GTK3_CALLBACKS_H
#define CAMORAMA_GTK3_CALLBACKS_H

#include "gtk_common_callbacks.h"

G_BEGIN_DECLS

gboolean gtk3_on_window_state_event(GtkWidget *widget,
                               GdkEventWindowState *event, cam_t *cam);
gboolean gtk_window_is_fullscreen(GtkWindow *window);
gboolean gtk3_draw_frame(GtkWidget *, cairo_t *, gpointer);
gboolean on_drawingarea_expose_event(GtkWidget *, GdkEventExpose *, cam_t *);
const gchar *gtk3_get_entry_text(GtkWidget *entry);
void gtk3_set_entry_text(GtkWidget *entry, const gchar *text);
gchar *gtk3_get_file_chooser_folder(GtkWidget *chooser);
void gtk3_set_file_chooser_folder(GtkWidget *chooser, const gchar *folder);
GtkWidget *gtk3_create_controls_window(GtkWidget *child, cam_t *cam);
GtkWidget *gtk3_create_control_button(video_controls_t *ctrl, gint32 value);
GtkWidget *gtk3_create_control_menu(video_controls_t *ctrl, gint32 value);
void gtk3_controls_box_append(GtkBox *box, GtkWidget *child);
void gtk3_update_controls_window(GtkWidget *window);
void gtk3_present_controls_window(GtkWindow *window);

G_END_DECLS

#endif                          /* !CAMORAMA_GTK3_CALLBACKS_H */
