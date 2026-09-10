#ifndef CAMORAMA_GTK_COMMON_CALLBACKS_H
#define CAMORAMA_GTK_COMMON_CALLBACKS_H

#include "v4l.h"
#include "fileio.h"

struct devnodes {
    char *fname;
    int minor;
    gboolean is_valid;
};

typedef struct {
    GType type;
    gchar *name;
} effect_menu_entry_t;

extern unsigned int n_devices, n_valid_devices;
extern struct devnodes *devices;

G_BEGIN_DECLS

void on_change_size_activate(GtkWidget * widget, cam_t *cam);
void on_quit_activate(GtkWidget *widget, cam_t *cam);
gboolean on_configure_event(GtkWidget *widget, GdkEvent *event, cam_t *cam);
int delete_event(GtkWidget *, gpointer data);
void cap_func(GtkWidget *, cam_t *);
void rcap_func(GtkWidget *, cam_t *);
void acap_func(GtkWidget *, cam_t *);
void set_sensitive(cam_t *);
void tt_enable_func(GtkWidget *, cam_t *);
void interval_change(GtkWidget *, cam_t *);
void ts_func(GtkWidget *, cam_t *);
void customstring_func(GtkWidget *, cam_t *);
void drawdate_func(GtkWidget *, cam_t *);
void append_func(GtkWidget *, cam_t *);
void rappend_func(GtkWidget *, cam_t *);
void jpg_func(GtkWidget *, cam_t *);
void png_func(GtkWidget *, cam_t *);
void ppm_func(GtkWidget *, cam_t *);
void rts_func(GtkWidget *, cam_t *);
void rjpg_func(GtkWidget *, cam_t *);
void rpng_func(GtkWidget *, cam_t *);
void rppm_func(GtkWidget *, cam_t *);
void on_preferences1_activate(GtkWidget *widget, gpointer user_data);
void on_about_activate(GtkWidget *widget, cam_t *cam);
void on_show_adjustments_activate(GtkWidget *button, cam_t *);
void on_show_effects_activate(GtkWidget *button, cam_t *);
void prefs_func(GtkWidget *, cam_t *);
gboolean delete_event_prefs_window(GtkWidget *widget, GdkEvent *event,
                                   cam_t *cam);
void capture_func2(GtkWidget *, cam_t *);
void capture_func(GtkWidget *, cam_t *);
void toggle_fullscreen(GtkWidget *, cam_t *);
gint timeout_capture_func(cam_t *);
gint fps(GtkWidget *);
gint timeout_func(cam_t *);
void edge_func1(GtkToggleButton *, gpointer);
void sobel_func(GtkToggleButton *, gpointer);
void fix_colour_func(GtkToggleButton *, char *);
void threshold_func1(GtkToggleButton *, gpointer);
void threshold_ch_func(GtkToggleButton *, gpointer);
void edge_func3(GtkToggleButton *, gpointer);
void mirror_func(GtkToggleButton *, gpointer);
void reichardt_func(GtkToggleButton *, gpointer);
void colour_func(GtkToggleButton *, gpointer);
void smooth_func(GtkToggleButton *, gpointer);
void negative_func(GtkToggleButton *, gpointer);
void on_scale1_drag_data_received(GtkScale *, cam_t *);
void on_status_show(GtkWidget *, cam_t *);
void show_controls(GtkWidget *widget, cam_t *cam);
void update_sliders(cam_t *cam);
void contrast_change(GtkScale *, cam_t *);
void brightness_change(GtkScale *, cam_t *);
void zoom_change(GtkScale *, cam_t *);
void colour_change(GtkScale *, cam_t *);
void hue_change(GtkScale *, cam_t *);
void wb_change(GtkScale *, cam_t *);
void gtk_common_update_image_scale(cam_t *cam, int width, int height);
void gtk_common_show_fullscreen_ui(cam_t *cam, gboolean fullscreen);
void gtk_common_set_window_icons(GtkWindow *window, GtkWindow *prefswindow);
void gtk_common_update_slider_value(video_controls_t *ctrl, cam_t *cam,
                                    gint32 value);
void gtk_common_clear_controls_window(cam_t *cam);
void gtk_common_setup_effects_popup(GtkTreeView *treeview);
void gtk_common_show_effects_popup(GtkTreeView *treeview, double x, double y);
void gtk_common_add_effect(GtkTreeView *treeview, GType filter_type);
void gtk_common_delete_effects(GtkTreeView *treeview);
const gchar *gtk_common_get_entry_text(GtkWidget *entry);
void gtk_common_set_entry_text(GtkWidget *entry, const gchar *text);
gchar *gtk_common_get_file_chooser_folder(GtkWidget *chooser);
void gtk_common_set_file_chooser_folder(GtkWidget *chooser,
                                        const gchar *folder);
gint gtk_common_dialog_run(GtkDialog *dialog);
void gtk_common_destroy_widget(GtkWidget *widget);
void set_image_scale(cam_t *cam);
void retrieve_video_dev(cam_t *cam);
int select_video_dev(cam_t *cam);
void on_change_camera(GtkWidget *widget, cam_t *cam);
void start_camera(cam_t *cam);

G_END_DECLS

#endif                          /* !CAMORAMA_GTK_COMMON_CALLBACKS_H */
