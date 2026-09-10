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
