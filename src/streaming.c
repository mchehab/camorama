#include "streaming.h"
#include "img_ffmpeg.h"

#include <string.h>
#include <unistd.h>

static gboolean consume_frame(cam_t *cam)
{
    struct cam_display_buffer *display_buffer;
    GdkPixbuf *pixbuf;
    int row, stride;
    GtkWidget *da;
    guchar *pixels;

    /* Switch capture <-> consume buffers and mark consume as owned */
    g_mutex_lock(&cam->display_mutex);
    cam->display_source = 0;
    if (g_atomic_int_get(&cam->stream_stop) ||
        cam->display_state != CAM_DISPLAY_READY) {
        g_mutex_unlock(&cam->display_mutex);
        return G_SOURCE_REMOVE;
    }

    display_buffer = &cam->display_buffers[cam->display_write_index ^ 1];
    cam->display_state = CAM_DISPLAY_OWNED;
    g_mutex_unlock(&cam->display_mutex);

    /*
     * The display buffer will be owned by consume_frame() until the full
     * chain is processed and the buffer is ready to be displayed or
     * captured.
     */
    if (cam->filter_chain)
        camorama_filter_chain_apply(cam->filter_chain, display_buffer->data,
                                    display_buffer->width,
                                    display_buffer->height, 3);

    pixbuf = gdk_pixbuf_new(GDK_COLORSPACE_RGB, FALSE, 8,
                            display_buffer->width, display_buffer->height);
    if (pixbuf) {
        pixels = gdk_pixbuf_get_pixels(pixbuf);
        stride = gdk_pixbuf_get_rowstride(pixbuf);

        for (row = 0; row < (int)display_buffer->height; row++)
            memcpy(pixels + row * stride,
                   display_buffer->data + row * display_buffer->rowstride,
                   display_buffer->rowstride);

        g_clear_object(&cam->pb);
        cam->pb = pixbuf;
    }

    /* Notify that consume buffer is not used anymore */
    g_mutex_lock(&cam->display_mutex);
    cam->display_state = CAM_DISPLAY_EMPTY;
    g_mutex_unlock(&cam->display_mutex);

    da = cam->da;
    if (!da && cam->xml)
        da = GTK_WIDGET(gtk_builder_get_object(cam->xml, "da"));
    if (GTK_IS_WIDGET(da))
        gtk_widget_queue_draw(da);

    return G_SOURCE_REMOVE;
}

static void publish_display(cam_t *cam)
{
    /*
     * Check if Gtk idle can consume the buffer. The idea here is to work
     * with two separate buffers:
     *
     * - the background task - stream_worker() - will keep a buffer slot;
     * - Gtk uses the other one to consume.
     *
     * If Gtk is fast enough, buffers will keep being swapped frame by frame,
     * but if Gtk or filter chain takes too long, the
     * capture part will keep using the same buffer.
     */
    if (!g_mutex_trylock(&cam->display_mutex))
        return;

    if (!g_atomic_int_get(&cam->stream_stop) &&
        cam->display_state != CAM_DISPLAY_OWNED) {
        cam->display_write_index ^= 1;
        cam->display_state = CAM_DISPLAY_READY;
        if (!cam->display_source)
            cam->display_source = g_idle_add((GSourceFunc)consume_frame, cam);
    }
    g_mutex_unlock(&cam->display_mutex);
}

static gpointer stream_worker(gpointer data)
{
    cam_t *cam = data;


    /*
     * Buffer swap is only done here, if the consumer is not using the
     * other buffer anymore.
     */
    while (!g_atomic_int_get(&cam->stream_stop)) {
        struct cam_display_buffer *display_write = &cam->display_buffers[cam->display_write_index];

        if (!cam_read(cam, display_write->data))
            continue;

        display_write->width = cam->width;
        display_write->height = cam->height;
        display_write->rowstride = cam->width * 3;
        display_write->generation = cam->stream_generation;
        display_write->sequence = ++cam->stream_sequence;

        publish_display(cam);
    }
    return NULL;
}

gboolean cam_stream_configure(cam_t *cam)
{
    size_t size;
    unsigned int i;

    g_return_val_if_fail(!cam->stream_thread, FALSE);

    if (!cam->width || !cam->height || cam->width > G_MAXINT / 3 / cam->height ||
        (size_t)cam->width > G_MAXSIZE / 3 / cam->height) {

        g_warning("Refusing invalid camera output size %ux%u",
                  cam->width, cam->height);
        return FALSE;
    }

    size = (size_t)cam->width * cam->height * 3;

    /*
     * Ensure that decoder doesn't contain data from previous state.
     * This is needed when using decoders like H.264.
     */
    img_ffmpeg_free_converter(&cam->converter);

    cam->stream_generation++;
    for (i = 0; i < G_N_ELEMENTS(cam->display_buffers); i++) {
        if (cam->display_buffers[i].capacity < size) {
            cam->display_buffers[i].data = g_realloc(cam->display_buffers[i].data, size);
            cam->display_buffers[i].capacity = size;
        }
    }

    g_mutex_lock(&cam->display_mutex);
    cam->display_write_index = 0;
    cam->display_state = CAM_DISPLAY_EMPTY;
    g_mutex_unlock(&cam->display_mutex);

    g_clear_object(&cam->pb);
    return TRUE;
}

void cam_stream_start(cam_t *cam)
{
    char byte;

    g_return_if_fail(!cam->stream_thread);
    while (cam->stream_wakeup[0] >= 0 &&
           read(cam->stream_wakeup[0], &byte, 1) == 1)
        ;
    g_atomic_int_set(&cam->stream_stop, FALSE);
    cam->stream_thread = g_thread_new("camorama-stream", stream_worker, cam);
}

void cam_stream_stop(cam_t *cam)
{
    if (!cam->stream_thread)
        return;

    g_atomic_int_set(&cam->stream_stop, TRUE);

    cam_cancel_read(cam);

    g_thread_join(cam->stream_thread);
    cam->stream_thread = NULL;

    g_mutex_lock(&cam->display_mutex);

    if (cam->display_source)
        g_source_remove(cam->display_source);

    cam->display_source = 0;
    cam->display_state = CAM_DISPLAY_EMPTY;

    g_mutex_unlock(&cam->display_mutex);
}

void cam_stream_cleanup(cam_t *cam)
{
    unsigned int i;

    cam_stream_stop(cam);

    for (i = 0; i < G_N_ELEMENTS(cam->display_buffers); i++) {
        g_free(cam->display_buffers[i].data);
        cam->display_buffers[i].data = NULL;
        cam->display_buffers[i].capacity = 0;
    }

    img_ffmpeg_free_converter(&cam->converter);
}
