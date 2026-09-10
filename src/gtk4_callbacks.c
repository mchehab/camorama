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

void on_window_fullscreen_changed(GtkWindow *window, GParamSpec *, cam_t *cam)
{
    gtk_common_show_fullscreen_ui(cam, gtk_window_is_fullscreen(window));
}

/*
 * Helper function to support filling the image filling rectangle
 */

void gtk4_draw_frame(GtkDrawingArea *, cairo_t *cr, int, int, gpointer data)
{
    cam_t *cam = data;
    const GdkRectangle rect = {
        .x = 0, .y = 0,
        .width = cam->width, .height = cam->height
    };

    if (!cam->pb)
        return;

    if (cam->scale > 0 && cam->scale != 1.f)
        cairo_scale(cr, cam->scale, cam->scale);

    gdk_cairo_set_source_pixbuf(cr, cam->pb, 0, 0);
    gdk_cairo_rectangle(cr, &rect);
    cairo_fill(cr);

    frames++;
    frames2++;
}
