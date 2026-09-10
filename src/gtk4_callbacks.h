#ifndef CAMORAMA_GTK4_CALLBACKS_H
#define CAMORAMA_GTK4_CALLBACKS_H

#include "gtk_common_callbacks.h"

G_BEGIN_DECLS

void gtk4_drawing_area_resize(GtkDrawingArea *, int, int, cam_t *cam);
void on_window_fullscreen_changed(GtkWindow *window, GParamSpec *pspec,
                                  cam_t *cam);
void gtk4_draw_frame(GtkDrawingArea *, cairo_t *, int, int, gpointer);
void gtk4_setup_effects_popup(GtkTreeView *treeview);
void gtk4_show_effects_popup(GtkTreeView *treeview, GMenuModel *model,
                             GActionGroup *actions, GPtrArray *entries,
                             double x, double y);
GtkWidget *gtk4_create_controls_window(GtkWidget *child, cam_t *cam);
GtkWidget *gtk4_create_control_button(video_controls_t *ctrl, gint32 value);
GtkWidget *gtk4_create_control_menu(video_controls_t *ctrl, gint32 value);
void gtk4_controls_box_append(GtkBox *box, GtkWidget *child);
void gtk4_update_controls_window(GtkWidget *window);
void gtk4_present_controls_window(GtkWindow *window);

G_END_DECLS

#endif                          /* !CAMORAMA_GTK4_CALLBACKS_H */
