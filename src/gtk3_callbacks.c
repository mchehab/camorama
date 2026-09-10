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
