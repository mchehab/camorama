#include "interface.h"
#include "support.h"
#include "fileio.h"
#include "camera-backend.h"

#include <errno.h>
#include <gio/gio.h>
#include <glib/gi18n.h>
#include <time.h>
#include <stdio.h>
#include <string.h>

#define CHAR_HEIGHT  11
#define CHAR_WIDTH   6
#define CHAR_START   4
#include "font_6x11.h"

static void free_snapshot(guchar *pixels, gpointer)
{
    g_free(pixels);
}

static GdkPixbuf *snapshot_display(cam_t *cam, gboolean timestamp)
{
    int width, height, stride, y;
    GdkPixbuf *source = cam->pb;
    GdkPixbuf *snapshot;
    guchar *pixels;

    /*
     * As this is running at gtk idle time as part of the display
     * pipeline, there's no need to take a mutex here, as the cam->pb
     * buffer won't be updated in background.
     */

    if (!source)
        return NULL;

    width = gdk_pixbuf_get_width(source);
    height = gdk_pixbuf_get_height(source);
    stride = gdk_pixbuf_get_rowstride(source);

    pixels = g_malloc((size_t)width * height * 3);
    for (y = 0; y < height; y++)
        memcpy(pixels + (size_t)y * width * 3,
               gdk_pixbuf_get_pixels(source) + y * stride, width * 3);

    if (timestamp)
        add_rgb_text(pixels, width, height, cam->ts_string, cam->date_format,
                     cam->usestring, cam->usedate);

    snapshot = gdk_pixbuf_new_from_data(pixels, GDK_COLORSPACE_RGB, FALSE, 8,
                                        width, height, width * 3,
                                        free_snapshot, NULL);
    if (!snapshot)
        free_snapshot(pixels, NULL);

    return snapshot;
}

/* add timestamp/text to image - "borrowed" from gspy */
int
add_rgb_text(guchar *image, int width, int height, char *cstring,
             char *format, gboolean str, gboolean date)
{
    time_t t;
    struct tm *tm;
    gchar line[128];
    guchar *ptr;
    int i, x, y, f, len;
    int total;
    gchar *image_label;

    if (!image || width < 4 + CHAR_WIDTH || height < CHAR_HEIGHT + 2)
        return 0;

    if (str == TRUE && date == TRUE) {
        image_label = g_strdup_printf("%s - %s", cstring, format);
    } else if (str == TRUE && date == FALSE) {
        image_label = g_strdup_printf("%s", cstring);
    } else if (str == FALSE && date == TRUE) {
        image_label = g_strdup_printf("%s", format);
    } else if (str == FALSE && date == FALSE) {
        return 0;
    } else {
        image_label = g_strdup("");
    }

    time(&t);
    tm = localtime(&t);

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-nonliteral"
    len = strftime(line, sizeof(line) - 1, image_label, tm);
#pragma GCC diagnostic pop
    g_free(image_label);

    len = MIN(len, (width - 4) / CHAR_WIDTH);

    for (y = 0; y < CHAR_HEIGHT; y++) {
        /* locate text in lower left corner of image */
        ptr = image + (size_t)3 * width * (height - CHAR_HEIGHT - 2 + y) + 12;

        /* loop for each character in the string */
        for (x = 0; x < len; x++) {
            /* locate the character in the fontdata array */
            f = fontdata[(guchar)line[x] * CHAR_HEIGHT + y];

            /* loop for each column of font data */
        for (i = CHAR_WIDTH - 1; i >= 0; i--) {
                /* write a black background under text */
                ptr[0] = 0;
                ptr[1] = 0;
                ptr[2] = 0;
                if (f & (CHAR_START << i)) {
                    /* white text */
                    total = ptr[0] + ptr[1] + ptr[2];
                    if (total / 3 < 128) {
                            ptr[0] = 255;
                            ptr[1] = 255;
                            ptr[2] = 255;
                    } else {
                            ptr[0] = 0;
                            ptr[1] = 0;
                            ptr[2] = 0;
                    }
                }
                ptr += 3;
            }
        }
    }
    return 1;
}

struct remote_snapshot {
    gchar *pixels;
    gsize size;
    GFile *destination;
};

static void free_remote_snapshot(gpointer data)
{
    struct remote_snapshot *snapshot = data;

    g_free(snapshot->pixels);
    g_object_unref(snapshot->destination);
    g_free(snapshot);
}

static void save_remote_snapshot(GTask *task, gpointer,
                                  gpointer data, GCancellable *cancellable)
{
    struct remote_snapshot *snapshot = data;
    GError *error = NULL;

    if (!g_file_replace_contents(snapshot->destination, snapshot->pixels,
                                  snapshot->size, NULL, FALSE,
                                  G_FILE_CREATE_REPLACE_DESTINATION,
                                  NULL, cancellable, &error))
        g_task_return_error(task, error);
    else
        g_task_return_boolean(task, TRUE);
}

static void remote_save_done(GObject *, GAsyncResult *result,
                             gpointer data)
{
    cam_t *cam = data;
    GError *error = NULL;

    cam->n_threads--;
    if (!g_task_propagate_boolean(G_TASK(result), &error)) {
        error_dialog(error->message);
        g_error_free(error);
    }
}

void remote_save(cam_t *cam)
{
    struct remote_snapshot *snapshot;
    GdkPixbuf *pb;
    GTask *task;
    GError *error = NULL;
    GDateTime *now;
    gchar *filename, *uri, *timestamp;
    const gchar *ext = cam->rsavetype == PNG ? "png" : "jpeg";

    /* This function and its completion callback run on the GTK thread. */
    if (!cam->rdir_ok || cam->n_threads)
        return;

    pb = snapshot_display(cam, cam->rtimestamp);
    if (!pb)
        return;

    snapshot = g_new0(struct remote_snapshot, 1);
    if (!gdk_pixbuf_save_to_buffer(pb, &snapshot->pixels, &snapshot->size,
                                    ext, &error, NULL)) {
        error_dialog(error->message);
        g_error_free(error);
        g_object_unref(pb);
        g_free(snapshot);
        return;
    }
    g_object_unref(pb);

    now = g_date_time_new_now_local();
    timestamp = g_date_time_format(now, "%Y%m%d-%H:%M:%S");
    if (cam->rtimefn)
        filename = g_strdup_printf("%s-%s-%03d.%s", cam->rcapturefile,
                                    timestamp,
                                    g_atomic_int_get(&cam->frame_number) % 1000,
                                    ext);
    else
        filename = g_strdup_printf("%s.%s", cam->rcapturefile, ext);
    uri = g_strdup_printf("%s/%s", cam->uri, filename);
    snapshot->destination = g_file_new_for_uri(uri);
    g_free(uri);
    g_free(filename);
    g_free(timestamp);
    g_date_time_unref(now);

    cam->n_threads++;
    task = g_task_new(NULL, NULL, remote_save_done, cam);
    g_task_set_task_data(task, snapshot, free_remote_snapshot);
    g_task_run_in_thread(task, save_remote_snapshot);
    g_object_unref(task);
}

struct mount_params {
    GFile *rdir_file;
    GMountOperation *mop;
    gchar *uri;
};

static void mount_cb(GObject *obj, GAsyncResult *res, gpointer user_data)
{
    cam_t *cam = user_data;
    gboolean ret;
    GError *err = NULL;

    ret = g_file_mount_enclosing_volume_finish(G_FILE(obj), res, &err);

    /* Ignore G_IO_ERROR_ALREADY_MOUNTED */
    if (g_error_matches(err, G_IO_ERROR, G_IO_ERROR_ALREADY_MOUNTED))
        ret = 1;

    if (ret) {
        cam->rdir_ok = TRUE;
        g_settings_set_string(cam->gc, CAM_SETTINGS_HOSTNAME, cam->host);
        g_settings_set_string(cam->gc, CAM_SETTINGS_REMOTE_PROTO, cam->proto);
        g_settings_set_string(cam->gc, CAM_SETTINGS_REMOTE_SAVE_DIR, cam->rdir);
        g_settings_set_string(cam->gc, CAM_SETTINGS_REMOTE_SAVE_FILE, cam->rcapturefile);
    } else {
        gchar *error_message = g_strdup_printf(_("An error occurred mounting %s:%s."),
                                               cam->uri, err->message);

        error_dialog(error_message);
        g_free(error_message);
    }
}

gchar *volume_uri(gchar *host, gchar *proto, gchar *rdir)
{
    return g_strdup_printf("%s://%s/%s", proto, host, rdir);
}

void umount_volume(cam_t *cam)
{
    /* Unmount previous volume */
    if (!cam->rdir_ok)
        return;

    cam->rdir_ok = FALSE;
    g_file_unmount_mountable_with_operation(cam->rdir_file,
                                            G_MOUNT_UNMOUNT_NONE,
                                            cam->rdir_mop, NULL,
                                            NULL, cam);
}

void mount_volume(cam_t *cam)
{
    /* Only try to mount if remote capture is enabled */
    if (!cam->rcap)
        return;

    /* Prepare a mount operation */
    cam->rdir_file = g_file_new_for_uri(cam->uri);
    if (cam->rdir_file)
        cam->rdir_mop = gtk_mount_operation_new(NULL);
    else
        cam->rdir_mop = NULL;

    if (!cam->rdir_mop) {
        gchar *error_message = g_strdup_printf(_("An error occurred accessing %s."),
                                               cam->uri);

        error_dialog(error_message);
        g_free(error_message);

        return;
    }

    g_file_mount_enclosing_volume(cam->rdir_file, G_MOUNT_MOUNT_NONE,
                                  cam->rdir_mop, NULL, mount_cb, cam);
}

int local_save(cam_t *cam)
{
    gchar *filename;
    const gchar *ext;
    time_t t;
    struct tm *tm;
    char timenow[64], *error_message;
    int len, mkd;
    gboolean pbs;
    GdkPixbuf *pb;

    /*
     * TODO: run gdk-pixbuf-query-loaders to get available image types
     */

    switch (cam->savetype) {
    case JPEG:
        ext = "jpeg";
        break;
    case PNG:
        ext = "png";
        break;
    default:
        ext = "jpeg";
    }

    time(&t);
    tm = localtime(&t);
    len = strftime(timenow, sizeof(timenow) - 1, "%Y%m%d-%H:%M:%S", tm);
    if (len < 0)
        timenow[0] = '\0';

    if (cam->debug == TRUE)
        fprintf(stderr, "time = %s\n", timenow);

    if (cam->timefn == TRUE)
        filename = g_strdup_printf("%s-%s-%03d.%s",
                                   cam->capturefile, timenow,
                                   g_atomic_int_get(&cam->frame_number) % 1000,
                                   ext);
    else
        filename = g_strdup_printf("%s.%s", cam->capturefile, ext);

    if (cam->debug == TRUE)
        fprintf(stderr, "filename = %s\n", filename);

    mkd = mkdir(cam->pixdir, 0777);

    if (cam->debug == TRUE)
        perror("create dir: ");

    if (mkd != 0 && errno != EEXIST) {
        error_message = g_strdup_printf(_("Could not create directory '%s'."),
                                        cam->pixdir);
        error_dialog(error_message);
        g_free(filename);
        g_free(error_message);
        return -1;
    }

    {
        gchar *path = g_build_filename(cam->pixdir, filename, NULL);

        g_free(filename);
        filename = path;
    }

    pb = snapshot_display(cam, cam->timestamp);
    if (!pb) {
        g_free(filename);
        return -1;
    }

    pbs = gdk_pixbuf_save(pb, filename, ext, NULL, NULL);
    g_object_unref(pb);
    if (pbs == FALSE) {
        error_message = g_strdup_printf(_("Could not save image '%s/%s'."),
                                        cam->pixdir, filename);
        error_dialog(error_message);
        g_free(filename);
        g_free(error_message);
        return -1;
    }

    g_free(filename);
    return 0;
}
