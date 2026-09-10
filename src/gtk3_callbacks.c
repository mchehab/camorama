#include "gtk3_callbacks.h"
#include "camorama-globals.h"

#include <config.h>

/*
 * Helper functions to support window area resize
 */

gboolean on_window_state_event(GtkWidget *, GdkEventWindowState *event,
                               cam_t *cam)
{
    gtk_common_show_fullscreen_ui(cam, event->new_window_state &
                                 GDK_WINDOW_STATE_FULLSCREEN);

    return GDK_EVENT_PROPAGATE;
}

gboolean gtk3_window_is_fullscreen(GtkWindow *window)
{
    GdkWindowState state;

    state = gdk_window_get_state(gtk_widget_get_window(GTK_WIDGET(window)));

    return state & GDK_WINDOW_STATE_FULLSCREEN;
}

/*
 * Helper function to set window icons
 */

void gtk3_set_window_icons(GtkWindow *window, GtkWindow *prefswindow)
{
    GdkPixbuf *icon;
    const gchar *icon_file = g_getenv("CAMORAMA_ICON_FILE");

    if (!icon_file)
        icon_file = PACKAGE_DATA_DIR
                    "/icons/hicolor/128x128/devices/camorama.png";

    icon = gdk_pixbuf_new_from_file(icon_file, NULL);
    gtk_window_set_default_icon(icon);
    gtk_window_set_icon(window, icon);
    gtk_window_set_icon(prefswindow, icon);
    g_clear_object(&icon);
}

/*
 * Helper function to support filling the image filling rectangle
 */

gboolean gtk3_draw_frame(GtkWidget *widget, cairo_t *cr, gpointer data)
{
    cam_t *cam = data;
    GdkWindow *window;
    cairo_surface_t *surface;
    const GdkRectangle rect = {
        .x = 0, .y = 0,
        .width = cam->width, .height = cam->height
    };

    if (!cam->pb)
        return GDK_EVENT_PROPAGATE;

    window = gtk_widget_get_window(widget);
    surface = gdk_cairo_surface_create_from_pixbuf(cam->pb, 1, window);

    if (cam->scale > 0 && cam->scale != 1.f)
        cairo_scale(cr, cam->scale, cam->scale);

    cairo_set_source_surface(cr, surface, 0, 0);
    gdk_cairo_rectangle(cr, &rect);
    cairo_fill(cr);
    cairo_surface_destroy(surface);

    frames++;
    frames2++;

    return GDK_EVENT_PROPAGATE;
}

/*
 * Helper functions to support the camera controls window
 */

static void gtk3_update_ctrl_button(GtkToggleButton *button,
                                    video_controls_t *ctrl)
{
    cam_t *cam = ctrl->cam;
    gint32 value;

    if (cam_get_control(cam, ctrl->id, &value))
        return;

    gtk_toggle_button_set_active(button, value);
    gtk_common_update_slider_value(ctrl, cam, value);
}

static void gtk3_change_ctrl_button(GtkToggleButton *button,
                                    video_controls_t *ctrl)
{
    cam_t *cam = ctrl->cam;
    gint32 value = gtk_toggle_button_get_active(button) ? 1 : 0;

    if (cam_set_control(cam, ctrl->id, &value)) {
        if (cam_get_control(cam, ctrl->id, &value))
            return;
        gtk_toggle_button_set_active(button, value);
    }
    gtk_common_update_slider_value(ctrl, cam, value);
}

static void gtk3_update_ctrl_menu(GtkComboBox *combo, video_controls_t *ctrl)
{
    gint32 value;
    unsigned int i;

    if (cam_get_control(ctrl->cam, ctrl->id, &value))
        return;

    for (i = 0; i < ctrl->menu_size; i++) {
        if (ctrl->menu[i].value == value) {
            gtk_combo_box_set_active(combo, i);
            return;
        }
    }
}

static void gtk3_change_ctrl_menu(GtkComboBox *combo, video_controls_t *ctrl)
{
    gint32 value;
    int pos = gtk_combo_box_get_active(combo);

    if (pos < 0)
        return;

    value = ctrl->menu[pos].value;
    if (cam_set_control(ctrl->cam, ctrl->id, &value))
        gtk3_update_ctrl_menu(combo, ctrl);
}

static void gtk3_send_control_update(GtkWidget *widget, gpointer)
{
    g_signal_emit_by_name(widget, "control_update");

    if (GTK_IS_CONTAINER(widget))
        gtk_container_forall(GTK_CONTAINER(widget),
                             gtk3_send_control_update, NULL);
}

static void gtk3_close_controls(GtkWidget *, cam_t *cam)
{
    gtk_common_clear_controls_window(cam);
}

GtkWidget *gtk3_create_controls_window(GtkWidget *child, cam_t *cam)
{
    GtkWidget *window = gtk_window_new(GTK_WINDOW_TOPLEVEL);

    gtk_container_add(GTK_CONTAINER(window), child);
    g_signal_connect(window, "destroy", G_CALLBACK(gtk3_close_controls), cam);

    return window;
}

GtkWidget *gtk3_create_control_button(video_controls_t *ctrl, gint32 value)
{
    GtkWidget *button = gtk_check_button_new_with_label(ctrl->name);

    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(button), value);
    g_signal_connect(button, "clicked",
                     G_CALLBACK(gtk3_change_ctrl_button), ctrl);
    g_signal_connect(button, "control_update",
                     G_CALLBACK(gtk3_update_ctrl_button), ctrl);

    return button;
}

GtkWidget *gtk3_create_control_menu(video_controls_t *ctrl, gint32 value)
{
    GtkWidget *combo = gtk_combo_box_text_new();
    unsigned int i;

    for (i = 0; i < ctrl->menu_size; i++) {
        gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(combo),
                                       ctrl->menu[i].name);
        if (ctrl->menu[i].value == value)
            gtk_combo_box_set_active(GTK_COMBO_BOX(combo), i);
    }
    g_signal_connect(combo, "changed",
                     G_CALLBACK(gtk3_change_ctrl_menu), ctrl);
    g_signal_connect(combo, "control_update",
                     G_CALLBACK(gtk3_update_ctrl_menu), ctrl);

    return combo;
}

void gtk3_controls_box_append(GtkBox *box, GtkWidget *child)
{
    gtk_container_add(GTK_CONTAINER(box), child);
}

void gtk3_update_controls_window(GtkWidget *window)
{
    gtk3_send_control_update(window, NULL);
}

void gtk3_present_controls_window(GtkWindow *window)
{
    gtk_widget_show_all(GTK_WIDGET(window));
}
