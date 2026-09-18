#include "v4l.h"

/* Keep the capture, filtering, and publication operation together here. */
gint timeout_func(cam_t *cam)
{
    unsigned char *pic_buf = cam_read(cam);
    GtkWidget *da;

    if (!pic_buf)
        return TRUE;

    g_mutex_lock(&cam->pixbuf_mutex);
    camorama_filter_chain_apply(cam->filter_chain, pic_buf,
                                cam->width, cam->height, 3);
    cam->pb = gdk_pixbuf_new_from_data(pic_buf, GDK_COLORSPACE_RGB, FALSE, 8,
                                       cam->width, cam->height,
                                       cam->width * 3, NULL, NULL);
    g_mutex_unlock(&cam->pixbuf_mutex);

    da = cam->da ? cam->da :
         GTK_WIDGET(gtk_builder_get_object(cam->xml, "da"));
    if (GTK_IS_WIDGET(da))
        gtk_widget_queue_draw(da);

    return TRUE;
}
