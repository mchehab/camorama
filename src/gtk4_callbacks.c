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

static cairo_surface_t *gtk4_create_frame_surface(GdkPixbuf *pixbuf)
{
    cairo_surface_t *surface;
    const guchar *source;
    guchar *data;
    gint source_stride;
    gint stride;
    gint width;
    gint height;
    gint x;
    gint y;

    width = gdk_pixbuf_get_width(pixbuf);
    height = gdk_pixbuf_get_height(pixbuf);
    source = gdk_pixbuf_read_pixels(pixbuf);
    source_stride = gdk_pixbuf_get_rowstride(pixbuf);
    surface = cairo_image_surface_create(CAIRO_FORMAT_RGB24, width, height);
    if (cairo_surface_status(surface) != CAIRO_STATUS_SUCCESS)
        return surface;

    data = cairo_image_surface_get_data(surface);
    stride = cairo_image_surface_get_stride(surface);

    for (y = 0; y < height; y++) {
        const guchar *source_pixel = source + y * source_stride;
        guint32 *pixel = (guint32 *)(data + y * stride);

        for (x = 0; x < width; x++) {
            pixel[x] = ((guint32)source_pixel[0] << 16) |
                       ((guint32)source_pixel[1] << 8) |
                       source_pixel[2];
            source_pixel += 3;
        }
    }

    cairo_surface_mark_dirty(surface);

    return surface;
}

void gtk4_draw_frame(GtkDrawingArea *, cairo_t *cr, int, int, gpointer data)
{
    cam_t *cam = data;
    cairo_surface_t *surface;
    const GdkRectangle rect = {
        .x = 0, .y = 0,
        .width = cam->width, .height = cam->height
    };

    if (!cam->pb)
        return;

    if (cam->scale > 0 && cam->scale != 1.f)
        cairo_scale(cr, cam->scale, cam->scale);

    surface = gtk4_create_frame_surface(cam->pb);
    if (cairo_surface_status(surface) != CAIRO_STATUS_SUCCESS) {
        cairo_surface_destroy(surface);
        return;
    }

    cairo_set_source_surface(cr, surface, 0, 0);
    cairo_rectangle(cr, rect.x, rect.y, rect.width, rect.height);
    cairo_fill(cr);
    cairo_surface_destroy(surface);

    frames++;
    frames2++;
}
