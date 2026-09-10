#include "interface.h"

#include "callbacks.h"
#include "filter.h"
#include "camorama-window.h"
#include "camorama-globals.h"
#include "support.h"
#include <config.h>

#include <glib/gi18n.h>
#include <locale.h>
#include <libv4l2.h>
#include <stdlib.h>

GtkWidget *prefswindow;
int frames, frames2, seconds;
GtkWidget *dentry, *entry2, *string_entry, *format_selection;
GtkWidget *host_entry, *protocol, *rdir_entry, *filename_entry;

static int ver = 0, max = 0, min;
static int half = 0, use_read = 0, use_userptr = 0, debug = 0;
static int dont_use_libv4l = 0;
static int disable_scaler = 0;
static gchar *video_dev = NULL;
static int x = 0, y = 0;
static int input = 0;

static void get_geometry(unsigned int *width, unsigned int *height)
{
    GdkRectangle geometry;
#if GTK_MAJOR_VERSION > 3
    GdkDisplay *display = gdk_display_get_default();
    GListModel *monitors = gdk_display_get_monitors(display);
    GdkMonitor *monitor = g_list_model_get_item(monitors, 0);
#else
    GdkWindow *root = gdk_screen_get_root_window(gdk_screen_get_default());
    GdkDisplay *display = gdk_display_get_default();
    GdkMonitor *monitor = gdk_display_get_monitor_at_window(display, root);
#endif

    gdk_monitor_get_geometry(monitor, &geometry);
#if GTK_MAJOR_VERSION > 3
    g_object_unref(monitor);
#endif
    *width = geometry.width;
    *height = geometry.height;
}

static void set_initial_window_size(cam_t *cam)
{
    unsigned int orig_height = cam->height;
    unsigned int orig_width = cam->width;
    unsigned int max_width, max_height;
    unsigned int width = 0, height = 0;
    GtkWidget *window;

    if (cam->scale <= 0)
        return;

    if (cam->has_window_geometry_settings) {
        width = g_settings_get_int(cam->gc, CAM_SETTINGS_WINDOW_WIDTH);
        height = g_settings_get_int(cam->gc, CAM_SETTINGS_WINDOW_HEIGHT);
    }

    if (!width)
        width = orig_width;
    else
        orig_width = width;


    if (!height)
        height = orig_height;
    else
        orig_height = height;

    get_geometry(&max_width, &max_height);

    if (width > max_width) {
        height = (guint64)height * max_width / width;
        width = max_width;
    }
    if (height > max_height) {
        width = (guint64)width * max_height / height;
        height = max_height;
    }

    if (cam->debug) {
        printf("Initial window geometry: %ux%u\n", width, height);

        if (orig_width != width || orig_height != height)
            printf("(Adjusting window geometry: %ux%u, monitor: %ux%u)\n",
                orig_width, orig_height,
                max_width, max_height);
    }

    window = GTK_WIDGET(gtk_builder_get_object(cam->xml, "main_window"));
    gtk_window_set_default_size(GTK_WINDOW(window), width, height);
}

static void remember_window_geometry(cam_t *cam)
{
    GtkWidget *window;

    if (cam->scale <= 0)
        return;

    window = GTK_WIDGET(gtk_builder_get_object(cam->xml, "main_window"));
    if (!gtk_widget_get_mapped(window))
        return;

    cam->window_width = gtk_widget_get_allocated_width(window);
    cam->window_height = gtk_widget_get_allocated_height(window);
}

static void save_window_geometry(cam_t *cam)
{
    if (!cam->has_window_geometry_settings || cam->window_width < 1 ||
        cam->window_height < 1)
        return;

    g_settings_set_int(cam->gc, CAM_SETTINGS_WINDOW_WIDTH,
                       cam->window_width);
    g_settings_set_int(cam->gc, CAM_SETTINGS_WINDOW_HEIGHT,
                       cam->window_height);
}

#if GTK_MAJOR_VERSION > 3
static void window_configured(GdkSurface *, int, int, cam_t *cam)
#else
static gboolean window_configured(GtkWidget *, GdkEvent *, cam_t *cam)
#endif
{
    remember_window_geometry(cam);
    save_window_geometry(cam);

#if GTK_MAJOR_VERSION < 4
    return GDK_EVENT_PROPAGATE;
#endif
}

static GOptionEntry options[] = {
    {"version", 'V', 0, G_OPTION_ARG_NONE, &ver,
     N_("show version and exit"), NULL},
    {"device", 'd', 0, G_OPTION_ARG_STRING, &video_dev,
     N_("v4l device to use"), NULL},
    {"debug", 'D', 0, G_OPTION_ARG_NONE, &debug,
     N_("enable debugging code"), NULL},
    {"width", 'x', 0, G_OPTION_ARG_INT, &x,
     N_("capture width"), NULL},
    {"height", 'y', 0, G_OPTION_ARG_INT, &y,
     N_("capture height"), NULL},
    {"max", 'M', 0, G_OPTION_ARG_NONE, &max,
     N_("maximum capture size"), NULL},
    {"min", 'm', 0, G_OPTION_ARG_NONE, &min,
     N_("minimum capture size"), NULL},
    {"half", 'H', 0, G_OPTION_ARG_NONE, &half,
     N_("middle capture size"), NULL},
    {"read", 'R', 0, G_OPTION_ARG_NONE, &use_read,
     N_("use read() rather than mmap()"), NULL},
    {"disable-scaler", 'S', 0, G_OPTION_ARG_NONE, &disable_scaler,
     N_("disable video scaler"), NULL},
    {"userptr", 'U', 0, G_OPTION_ARG_NONE, &use_userptr,
     N_("use userptr pointer rather than mmap()"), NULL},
    {"dont-use-libv4l2", 'D', 0, G_OPTION_ARG_NONE, &dont_use_libv4l,
     N_("use userptr pointer rather than mmap()"), NULL},
    {"input", 'i', 0, G_OPTION_ARG_INT, &input,
     N_("v4l device input to use"), NULL},
    {NULL}
};

static void close_app(GtkWidget *, cam_t *cam)
{
    if (cam->debug && cam->scale > 0)
        printf("Window geometry at close: %dx%d\n",
               cam->window_width, cam->window_height);

    save_window_geometry(cam);
    g_settings_sync();

    if (cam->idle_id)
        g_source_remove(cam->idle_id);

    if (cam->read == FALSE) {
        if (cam->userptr)
            stop_streaming_userptr(cam);
        else if (cam->read == FALSE)
            stop_streaming(cam);
    }

    if (cam->screensaver_inhibit_cookie)
        gtk_application_uninhibit(cam->app,
                                  cam->screensaver_inhibit_cookie);

    cam_close(cam);

    if (cam->timeout_id)
        g_source_remove(cam->timeout_id);

    if (cam->timeout_fps_id)
        g_source_remove(cam->timeout_fps_id);

    g_free(cam->video_dev);
    g_free(cam->pixdir);
    g_free(cam->host);
    g_free(cam->proto);
    g_free(cam->rdir);
    g_free(cam->rcapturefile);
    g_free(cam->ts_string);
    free(cam->pic_buf);
    free(cam->tmp);

    g_object_unref (G_OBJECT (cam->xml));
    g_object_unref (G_OBJECT (cam->gc));

}

static void activate(GtkApplication *app)
{
    GSettingsSchema *schema = NULL;
    cam_t *cam = g_new0(cam_t, 1);
    const gchar *data_dir;
    g_autofree gchar *ui_file = NULL;
    GtkWidget *widget, *window;
    unsigned int i;

    /* set non-zero default values */
    cam->size = PICHALF;
    cam->scale = 1.f;
    cam->dev = -1;
    cam->input = input;
    cam->app = app;
    g_mutex_init(&cam->remote_save_mutex);
    g_mutex_init(&cam->pixbuf_mutex);
    g_mutex_init(&cam->control_win_mutex);

    /* gtk is initialized now */
    camorama_filters_init();

    cam->debug = debug;

    if (ver) {
        fprintf(stderr, _("\n\nCamorama version %s\n\n"), PACKAGE_VERSION);
        exit(0);
    }
    if (max)
        cam->size = PICMAX;

    if (min)
        cam->size = PICMIN;

    if (half)
        cam->size = PICHALF;

    if (!dont_use_libv4l)
        cam->use_libv4l = TRUE;

    if (use_read) {
        printf("Forcing read mode\n");
        cam->read = TRUE;
    } else if (use_userptr) {
        printf("Forcing userptr mode\n");
        cam->userptr = TRUE;
        cam->use_libv4l = FALSE;
    }

    if (disable_scaler) {
        printf("Disabling auto scaling\n");
	cam->scale = -1.;
    }

    cam->xml = gtk_builder_new();

    data_dir = g_getenv("CAMORAMA_DATA_DIR");
    if (data_dir)
        ui_file = g_build_filename(data_dir, CAMORAMA_UI, NULL);
    else
        ui_file = g_build_filename(PACKAGE_DATA_DIR, "camorama",
                                   CAMORAMA_UI, NULL);

    if (!gtk_builder_add_from_file(cam->xml, ui_file, NULL)) {
        error_dialog(_("Couldn't load builder file"));
        exit(1);
    }

    retrieve_video_dev(cam);

    cam->gc = g_settings_new(CAM_SETTINGS_SCHEMA);

    g_object_get(cam->gc, "settings-schema", &schema, NULL);
    cam->has_window_geometry_settings = g_settings_schema_has_key(schema,
                                                                  CAM_SETTINGS_WINDOW_WIDTH);

    cam->has_window_geometry_settings &= g_settings_schema_has_key(schema,
                                                                   CAM_SETTINGS_WINDOW_HEIGHT);
    g_settings_schema_unref(schema);

    if (!video_dev) {
        gchar const *gconf_device = g_settings_get_string(cam->gc,
                                                          CAM_SETTINGS_DEVICE);
        if (gconf_device)
            cam->video_dev = g_strdup(gconf_device);
        else
            cam->video_dev = NULL;
    } else {
        cam->video_dev = g_strdup(video_dev);
    }

    if (cam->video_dev) {
        for (i = 0; i < n_devices; i++)
            if (!strcmp(cam->video_dev, devices[i].fname) && devices[i].is_valid)
                break;

        /* Not found, or doesn't work. Falling back to the first device */
        if (i == n_devices) {
            char *msg;

            if (n_valid_devices == 1)
                msg = g_strdup_printf(_("%s not found. Falling back to %s"),
                                     cam->video_dev, devices[0].fname);
            else
                msg = g_strdup_printf(_("%s not found."),
                                     cam->video_dev);
            error_dialog(msg);
            g_free(msg);

            /* Ask user or get the only one device, if it is the case */
            select_video_dev(cam);
        }
    } else {
        cam->video_dev = devices[0].fname;
    }

    if (cam->debug)
        printf("Using videodev: %s\n", cam->video_dev);

    cam->date_format = (char *) "%Y-%m-%d %H:%M:%S";

    cam->pixdir = g_settings_get_string(cam->gc, CAM_SETTINGS_SAVE_DIR);
    /* Deal with the save-dir default value starting with "~/" */
    if (cam->pixdir && cam->pixdir[0] == '~' && cam->pixdir[1] == '/') {
        gchar *old = cam->pixdir;
        cam->pixdir = g_strdup_printf("%s/%s", g_get_home_dir(), old + 2);
        g_free(old);
    }
    cam->capturefile = g_settings_get_string(cam->gc, CAM_SETTINGS_SAVE_FILE);
    cam->savetype = g_settings_get_int(cam->gc, CAM_SETTINGS_FILE_TYPE);
    cam->host = g_settings_get_string(cam->gc, CAM_SETTINGS_HOSTNAME);
    cam->proto = g_settings_get_string(cam->gc, CAM_SETTINGS_REMOTE_PROTO);
    cam->rdir = g_settings_get_string(cam->gc, CAM_SETTINGS_REMOTE_SAVE_DIR);
    cam->rcapturefile = g_settings_get_string(cam->gc,
                                              CAM_SETTINGS_REMOTE_SAVE_FILE);
    cam->rsavetype = g_settings_get_int(cam->gc,
                                        CAM_SETTINGS_REMOTE_FILE_TYPE);
    cam->ts_string = g_settings_get_string(cam->gc,
                                           CAM_SETTINGS_TIMESTAMP_STRING);
    cam->timestamp = g_settings_get_boolean(cam->gc, CAM_SETTINGS_TIMESTAMP);
    cam->rtimestamp = g_settings_get_boolean(cam->gc, CAM_SETTINGS_REMOTE_TIMESTAMP);

    cam->cap = g_settings_get_boolean(cam->gc, CAM_SETTINGS_LOCAL_CAPTURE);
    cam->rcap = g_settings_get_boolean(cam->gc, CAM_SETTINGS_REMOTE_CAPTURE);
    cam->timefn = g_settings_get_boolean(cam->gc, CAM_SETTINGS_LOCAL_APPEND_TS);
    cam->rtimefn = g_settings_get_boolean(cam->gc,
                                         CAM_SETTINGS_REMOTE_APPEND_TS);
    cam->usestring = g_settings_get_boolean(cam->gc,
                                           CAM_SETTINGS_USE_CUSTOM_STRING);
    cam->usedate = g_settings_get_boolean(cam->gc, CAM_SETTINGS_DRAWDATE);
    cam->acap = g_settings_get_boolean(cam->gc, CAM_SETTINGS_AUTO_CAPTURE);
    cam->timeout_interval = g_settings_get_int(cam->gc,
                                               CAM_SETTINGS_AUTO_CAPTURE_INTERVAL);
    cam->show_adjustments = g_settings_get_boolean(cam->gc,
                                                   CAM_SETTINGS_SHOW_ADJUSTMENTS);
    cam->show_effects = g_settings_get_boolean(cam->gc,
                                               CAM_SETTINGS_SHOW_EFFECTS);
    if (x)
        cam->width = x;
    else
        cam->width = g_settings_get_int(cam->gc, CAM_SETTINGS_WIDTH);

    if (y)
        cam->height = y;
    else
        cam->height = g_settings_get_int(cam->gc, CAM_SETTINGS_HEIGHT);

    set_initial_window_size(cam);
    start_camera(cam);

    cam->screensaver_inhibit_cookie =
        gtk_application_inhibit(cam->app, NULL,
                                GTK_APPLICATION_INHIBIT_IDLE,
                                _("Capturing video"));

    window = GTK_WIDGET(gtk_builder_get_object(cam->xml, "main_window"));
#if GTK_MAJOR_VERSION < 4
    g_signal_connect(window, "configure-event",
                     G_CALLBACK(window_configured), cam);
#endif

    load_interface(cam);

#if GTK_MAJOR_VERSION > 3
    GdkSurface *surface = gtk_native_get_surface(GTK_NATIVE(window));

    if (surface)
        g_signal_connect(surface, "layout",
                         G_CALLBACK(window_configured), cam);
#endif

    widget = GTK_WIDGET(gtk_builder_get_object(cam->xml, "da"));

    g_signal_connect(G_OBJECT(widget), "draw", G_CALLBACK(draw_callback), cam);

    g_signal_connect(G_OBJECT(window), "destroy",
                     G_CALLBACK (close_app), cam);

    gtk_widget_show(widget);

    cam->timeout_fps_id = g_timeout_add(2000, (GSourceFunc) fps, cam->status);
}

int main(int argc, char *argv[])
{
    GtkApplication *app;
    gint status;

    bindtextdomain(PACKAGE_NAME, PACKAGE_LOCALE_DIR);
    bind_textdomain_codeset(PACKAGE_NAME, "UTF-8");
    textdomain(PACKAGE_NAME);

#if GLIB_CHECK_VERSION(2, 74, 0)
    app = gtk_application_new(NULL, G_APPLICATION_DEFAULT_FLAGS);
#else
    app = gtk_application_new(NULL, G_APPLICATION_FLAGS_NONE);
#endif
    g_application_add_main_option_entries(G_APPLICATION(app), options);
    g_signal_connect(app, "activate", G_CALLBACK(activate), NULL);

    status = g_application_run(G_APPLICATION(app), argc, argv);

    g_object_unref(app);

    return status;
}
