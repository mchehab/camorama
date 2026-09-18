#ifndef CAMORAMA_GTK4_CALLBACKS_H
#define CAMORAMA_GTK4_CALLBACKS_H

#include "gtk_common_callbacks.h"

G_BEGIN_DECLS

void gtk4_drawing_area_resize(GtkDrawingArea *, int, int, cam_t *cam);
void on_window_fullscreen_changed(GtkWindow *window, GParamSpec *pspec,
                                  cam_t *cam);
void gtk4_set_window_icons(GtkWindow *window, GtkWindow *prefswindow);
void gtk4_draw_frame(GtkDrawingArea *, cairo_t *, int, int, gpointer);
const gchar *gtk4_get_entry_text(GtkWidget *entry);
void gtk4_set_entry_text(GtkWidget *entry, const gchar *text);
gchar *gtk4_get_file_chooser_folder(GtkWidget *chooser);
void gtk4_set_file_chooser_folder(GtkWidget *chooser, const gchar *folder);
void gtk4_choice_setup(GtkWidget *choice);
void gtk4_choice_append(GtkWidget *choice, const gchar *text);
void gtk4_choice_set_active(GtkWidget *choice, guint index);
gint gtk4_choice_get_active(GtkWidget *choice);
gchar *gtk4_choice_get_active_text(GtkWidget *choice);
gint gtk4_window_run(GtkWindow *window, GtkWidget *response_widget);
int gtk4_error_dialog(const gchar *message);
void gtk4_destroy_widget(GtkWidget *widget);
gboolean gtk4_close_prefs_window(GtkWindow *window, cam_t *cam);
void gtk4_box_append(GtkBox *box, GtkWidget *child);
GList *gtk4_get_children(GtkWidget *widget);
GtkWidget *gtk4_create_controls_window(GtkWidget *child, cam_t *cam);
GtkWidget *gtk4_create_control_button(video_controls_t *ctrl, gint32 value);
GtkWidget *gtk4_create_control_menu(video_controls_t *ctrl, gint32 value);
void gtk4_update_controls_window(GtkWidget *window);
void gtk4_present_controls_window(GtkWindow *window);

G_END_DECLS

#endif                          /* !CAMORAMA_GTK4_CALLBACKS_H */
