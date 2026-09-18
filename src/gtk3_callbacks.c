#include "gtk3_callbacks.h"
#include "camorama-globals.h"

#include <config.h>

/*
 *Implement a Gtk3 function emulating Gtk4 gtk_window_is_fullscreen().
 *
 * The option here is to use the same name as Gtk4, as it helps reducing
 * one #if macro check from the code.
 */
gboolean gtk_window_is_fullscreen(GtkWindow *window)
{
    GdkWindowState state;

    state = gdk_window_get_state(gtk_widget_get_window(GTK_WIDGET(window)));

    return state & GDK_WINDOW_STATE_FULLSCREEN;
}

/*
 * Helper functions to support window area resize
 */
gboolean gtk3_on_window_state_event(GtkWidget *, GdkEventWindowState *event,
                               cam_t *cam)
{
    gtk_common_show_fullscreen_ui(cam, event->new_window_state &
                                 GDK_WINDOW_STATE_FULLSCREEN);

    return GDK_EVENT_PROPAGATE;
}

/*
 * Helper function to set window icons
 */

void gtk3_set_window_icons(GtkWindow *window, GtkWindow *prefswindow)
{
    const gchar *icon_file = g_getenv("CAMORAMA_ICON_FILE");
    GdkPixbuf *icon;

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
    cairo_surface_t *surface;
    cam_t *cam = data;
    GdkWindow *window;
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
 * Helper functions to support preference widgets
 */

const gchar *gtk3_get_entry_text(GtkWidget *entry)
{
    return gtk_entry_get_text(GTK_ENTRY(entry));
}

void gtk3_set_entry_text(GtkWidget *entry, const gchar *text)
{
    gtk_entry_set_text(GTK_ENTRY(entry), text);
}

gchar *gtk3_get_file_chooser_folder(GtkWidget *chooser)
{
    return gtk_file_chooser_get_current_folder(GTK_FILE_CHOOSER(chooser));
}

void gtk3_set_file_chooser_folder(GtkWidget *chooser, const gchar *folder)
{
    gtk_file_chooser_set_current_folder(GTK_FILE_CHOOSER(chooser), folder);
}

/*
 * Helper functions to support dialogs
 */

gint gtk3_dialog_run(GtkDialog *dialog)
{
    return gtk_dialog_run(dialog);
}

void gtk3_destroy_widget(GtkWidget *widget)
{
    gtk_widget_destroy(widget);
}

gboolean gtk3_close_prefs_window(GtkWidget *widget, GdkEvent *, cam_t *cam)
{
    prefs_func(widget, cam);

    return GDK_EVENT_STOP;
}

int gtk3_error_dialog(char *message)
{
    GtkApplication *app;
    GtkWindow *parent = NULL;
    GtkWidget *dialog;
    int test;

    app = GTK_APPLICATION(g_application_get_default());
    if (GTK_IS_APPLICATION(app))
        parent = gtk_application_get_active_window(app);

    /*
     * Startup errors can happen before the main window has been presented.
     * GTK warns when a dialog is mapped without a transient parent, so keep
     * those errors on stderr instead of creating an orphaned dialog.
     */
    if (!parent) {
        g_printerr("Camorama: %s\n", message);
        return 0;
    }

    dialog = gtk_message_dialog_new(parent,
                                    GTK_DIALOG_DESTROY_WITH_PARENT,
                                    GTK_MESSAGE_ERROR,
                                    GTK_BUTTONS_CLOSE, "%s", message);

    test = gtk3_dialog_run(GTK_DIALOG(dialog));
    gtk_common_destroy_widget(dialog);
    return test;
}

/*
 * Helper functions to support container operations
 */

void gtk3_box_append(GtkBox *box, GtkWidget *child)
{
    gtk_container_add(GTK_CONTAINER(box), child);
}

GList *gtk3_get_children(GtkWidget *widget)
{
    if (!GTK_IS_CONTAINER(widget))
        return NULL;

    return gtk_container_get_children(GTK_CONTAINER(widget));
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

void gtk3_update_controls_window(GtkWidget *window)
{
    gtk3_send_control_update(window, NULL);
}

void gtk3_present_controls_window(GtkWindow *window)
{
    gtk_widget_show_all(GTK_WIDGET(window));
}
