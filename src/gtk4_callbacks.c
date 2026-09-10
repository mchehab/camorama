#include "gtk4_callbacks.h"
#include "camorama-globals.h"

/*
 * Helper functions to support window area resize
 */

void gtk4_drawing_area_resize(GtkDrawingArea *, int width, int height,
                              cam_t *cam)
{
    gtk_common_update_image_scale(cam, width, height);
}

void gtk4_fullscreen_changed(GtkWindow *window, GParamSpec *, cam_t *cam)
{
    gtk_common_show_fullscreen_ui(cam, gtk_window_is_fullscreen(window));
}

/*
 * Helper function to support filling the image filling rectangle
 */

void gtk4_draw_frame(GtkDrawingArea *, cairo_t *cr, int, int, gpointer data)
{
    #error "Can't draw yet with gtk > 3.94"
}
