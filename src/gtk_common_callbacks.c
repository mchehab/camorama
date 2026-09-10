#include "gtk_common_callbacks.h"
#if GTK_MAJOR_VERSION < 4
#include "gtk3_callbacks.h"
#else
#include "gtk4_callbacks.h"
#endif
#include "interface.h"
#include "support.h"
#include "filter.h"

#include <assert.h>
#include <ftw.h>
#include <glib/gi18n.h>
#include <config.h>
#include <pthread.h>
#include <libv4l2.h>
#include <sys/sysmacros.h>

#define GPL_LICENSE \
    "GPL version 2.\n\n"                                                      \
    "This program is free software; you can redistribute it and/or modify it" \
    " under the terms of the GNU General Public License as published by the " \
    "Free Software Foundation version 2 of the License.\n\n"                  \
    "This program is distributed in the hope that it will be useful, but "    \
    "WITHOUT ANY WARRANTY; without even the implied warranty of "             \
    "MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU "      \
    "General Public License for more details."

static GtkWidget *about = NULL;

extern GtkWidget *prefswindow;
//extern state func_state;
//extern gint effect_mask;
extern int frames;
extern int frames2;
extern int seconds;
extern GtkWidget *dentry, *entry2, *string_entry;
extern GtkWidget *host_entry, *protocol, *rdir_entry, *filename_entry;
extern const gchar *const protos[3];

/*
 * pref callbacks
 */

void ts_func(GtkWidget *rb, cam_t *cam)
{
    cam->timestamp = gtk_toggle_button_get_active((GtkToggleButton *) rb);
    g_settings_set_boolean(cam->gc, CAM_SETTINGS_TIMESTAMP, cam->timestamp);
}

void customstring_func(GtkWidget *rb, cam_t *cam)
{
    cam->usestring = gtk_toggle_button_get_active((GtkToggleButton *) rb);
    g_settings_set_boolean(cam->gc, CAM_SETTINGS_USE_CUSTOM_STRING,
                           cam->usestring);
    gtk_widget_set_sensitive(GTK_WIDGET(gtk_builder_get_object(cam->xml, "string_entry")),
                             cam->usestring);
}

void drawdate_func(GtkWidget *rb, cam_t *cam)
{
    cam->usedate = gtk_toggle_button_get_active((GtkToggleButton *) rb);
    g_settings_set_boolean(cam->gc, CAM_SETTINGS_DRAWDATE, cam->usedate);
}

void append_func(GtkWidget *rb, cam_t *cam)
{
    cam->timefn = gtk_toggle_button_get_active((GtkToggleButton *) rb);
    g_settings_set_boolean(cam->gc, CAM_SETTINGS_LOCAL_APPEND_TS, cam->timefn);

}

void rappend_func(GtkWidget *rb, cam_t *cam)
{
    cam->rtimefn = gtk_toggle_button_get_active((GtkToggleButton *) rb);
    g_settings_set_boolean(cam->gc, CAM_SETTINGS_REMOTE_APPEND_TS, cam->rtimefn);

}

void jpg_func(GtkWidget *, cam_t *cam)
{
    cam->savetype = JPEG;
    g_settings_set_int(cam->gc, CAM_SETTINGS_FILE_TYPE, cam->savetype);

}

void png_func(GtkWidget *, cam_t *cam)
{
    cam->savetype = PNG;
    g_settings_set_int(cam->gc, CAM_SETTINGS_FILE_TYPE, cam->savetype);
}

void ppm_func(GtkWidget *, cam_t *cam)
{
    cam->savetype = PPM;
    g_settings_set_int(cam->gc, CAM_SETTINGS_FILE_TYPE, cam->savetype);
}

void set_sensitive(cam_t *cam)
{
    gtk_widget_set_sensitive(GTK_WIDGET
                             (gtk_builder_get_object(cam->xml, "table4")),
                             cam->cap);
    gtk_widget_set_sensitive(GTK_WIDGET(gtk_builder_get_object(cam->xml, "appendbutton")),
                             cam->cap);
    gtk_widget_set_sensitive(GTK_WIDGET(gtk_builder_get_object
                              (cam->xml, "tsbutton")), cam->cap);
    gtk_widget_set_sensitive(GTK_WIDGET(gtk_builder_get_object(cam->xml, "jpgb")),
                             cam->cap);
    gtk_widget_set_sensitive(GTK_WIDGET(gtk_builder_get_object(cam->xml, "pngb")),
                             cam->cap);
    gtk_widget_set_sensitive(GTK_WIDGET(gtk_builder_get_object(cam->xml, "table5")),
                             cam->rcap);
    gtk_widget_set_sensitive(GTK_WIDGET(gtk_builder_get_object(cam->xml, "timecb")),
                             cam->rcap);
    gtk_widget_set_sensitive(GTK_WIDGET(gtk_builder_get_object(cam->xml, "tsbutton2")),
                             cam->rcap);
    gtk_widget_set_sensitive(GTK_WIDGET(gtk_builder_get_object(cam->xml, "fjpgb")),
                             cam->rcap);
    gtk_widget_set_sensitive(GTK_WIDGET(gtk_builder_get_object(cam->xml, "fpngb")),
                             cam->rcap);
    gtk_widget_set_sensitive(GTK_WIDGET(gtk_builder_get_object(cam->xml, "hbox20")),
                             cam->acap);
}

void cap_func(GtkWidget *rb, cam_t *cam)
{
    cam->cap = gtk_toggle_button_get_active((GtkToggleButton *) rb);

    g_settings_set_boolean(cam->gc, CAM_SETTINGS_LOCAL_CAPTURE, cam->cap);
    set_sensitive(cam);
}

void rcap_func(GtkWidget *rb, cam_t *cam)
{
    cam->rcap = gtk_toggle_button_get_active((GtkToggleButton *) rb);

    g_settings_set_boolean(cam->gc, CAM_SETTINGS_REMOTE_CAPTURE, cam->rcap);
    set_sensitive(cam);

}

void acap_func(GtkWidget *rb, cam_t *cam)
{
    cam->acap = gtk_toggle_button_get_active((GtkToggleButton *) rb);

    g_settings_set_boolean(cam->gc, CAM_SETTINGS_AUTO_CAPTURE, cam->acap);

    if (cam->acap == TRUE) {
        cam->timeout_id = g_timeout_add(cam->timeout_interval,
                                        (GSourceFunc) timeout_capture_func, cam);
        if (cam->debug == TRUE) {
            printf("add autocap - %d -  timeout_interval = %d\n",
                   cam->timeout_id, cam->timeout_interval);
        }
    } else {
        if (cam->debug == TRUE) {
            printf("remove autocap - %d -  timeout_interval = %d\n",
                   cam->timeout_id, cam->timeout_interval);
        }
        g_source_remove(cam->timeout_id);
    }
    set_sensitive(cam);
}

void interval_change(GtkWidget *sb, cam_t *cam)
{
    cam->timeout_interval = gtk_spin_button_get_value((GtkSpinButton *) sb) * 60000;
    g_settings_set_int(cam->gc, CAM_SETTINGS_AUTO_CAPTURE_INTERVAL,
                       cam->timeout_interval);
    if (cam->acap == TRUE) {
        if (cam->debug == TRUE) {
            printf("interval_change; old timeout_id = %d old interval = %d\n",
                   cam->timeout_id, cam->timeout_interval);
        }
        g_source_remove(cam->timeout_id);
        cam->timeout_id = g_timeout_add(cam->timeout_interval,
                                        (GSourceFunc) timeout_capture_func, cam);
        if (cam->debug == TRUE) {
            printf("new timeout_id = %d, new interval = %d\n",
                   cam->timeout_id, cam->timeout_interval);
        }
    }
}

void rjpg_func(GtkWidget *, cam_t *cam)
{
    cam->rsavetype = JPEG;
    g_settings_set_int(cam->gc, CAM_SETTINGS_REMOTE_FILE_TYPE, cam->rsavetype);

}

void rpng_func(GtkWidget *, cam_t *cam)
{
    cam->rsavetype = PNG;
    g_settings_set_int(cam->gc, CAM_SETTINGS_REMOTE_FILE_TYPE, cam->rsavetype);
}

void rppm_func(GtkWidget *, cam_t *cam)
{
    cam->rsavetype = PPM;
    g_settings_set_int(cam->gc, CAM_SETTINGS_REMOTE_FILE_TYPE, cam->rsavetype);
}

void rts_func(GtkWidget *rb, cam_t *cam)
{
    cam->rtimestamp = gtk_toggle_button_get_active((GtkToggleButton *) rb);
    g_settings_set_boolean(cam->gc, CAM_SETTINGS_REMOTE_TIMESTAMP,
                           cam->rtimestamp);
}

static int apply_remote_pref(cam_t *cam)
{
    int index;
    gchar *host, *rdir, *proto, *rfile, *uri;

    if (!strlen(gtk_entry_get_text((GtkEntry *) host_entry)))
        return 0;

    index = gtk_combo_box_get_active(GTK_COMBO_BOX(protocol));
    if (index < 0 || index >= (int)G_N_ELEMENTS(protos))
        return 0;

    host = g_strdup(gtk_entry_get_text((GtkEntry *) host_entry));
    rdir = g_strdup(gtk_entry_get_text((GtkEntry *) rdir_entry));
    proto = g_strdup(protos[index]);
    rfile = g_strdup(gtk_entry_get_text((GtkEntry *) filename_entry));

    if (!host || !proto || !rdir || !rfile) {
        if (host)
            g_free(host);
        if (proto)
            g_free(proto);
        if (rdir)
            g_free(rdir);
        if (rfile)
            g_free(rfile);
        return 0;
    }

    uri = volume_uri(host, proto, rdir);

    if (cam->rdir_ok) {
        /* unmount/mount can spend time. Do only if URI changed */
        if (strcmp(uri, cam->uri)) {
            umount_volume(cam);
        } else {
            g_free(host);
            g_free(proto);
            g_free(rdir);
            g_free(rfile);
            g_free(uri);

            return 0;
        }
    }

    if (cam->host)
        g_free(cam->host);
    if (cam->proto)
        g_free(cam->proto);
    if (cam->rdir)
        g_free(cam->rdir);
    if (cam->rcapturefile)
        g_free(cam->rcapturefile);
    if (cam->uri)
        g_free(cam->uri);

    cam->host = host;
    cam->rdir = rdir;
    cam->proto = proto;
    cam->rcapturefile = rfile;
    cam->uri = uri;

    mount_volume(cam);

    return 1;
}

/*
 * apply preferences
 */
void prefs_func(GtkWidget *, cam_t *cam)
{
    gchar *pixdir;

    pixdir = gtk_file_chooser_get_current_folder((GtkFileChooser *) dentry);
    if (pixdir) {
        if (cam->pixdir)
            g_free(cam->pixdir);
        cam->pixdir = pixdir;
        g_settings_set_string(cam->gc, CAM_SETTINGS_SAVE_DIR, cam->pixdir);
    } else {
        if (cam->debug == TRUE)
            fprintf(stderr, "null directory\ndirectory unchanged.");
    }

    if (!apply_remote_pref(cam) && cam->debug == TRUE)
        fprintf(stderr,
                "remote directory params wrong\ndirectory unchanged.");

    /*
     * this is stupid, even if the string is empty, it will not return NULL
     */
    if (strlen(gtk_entry_get_text((GtkEntry *) entry2)) > 0) {
        if(cam->capturefile)
            g_free(cam->capturefile);
        cam->capturefile = g_strdup(gtk_entry_get_text((GtkEntry *) entry2));
        g_settings_set_string(cam->gc, CAM_SETTINGS_SAVE_FILE,
                              cam->capturefile);
    }

    if (strlen(gtk_entry_get_text((GtkEntry *) string_entry)) > 0) {
        if (cam->ts_string)
            g_free(cam->ts_string);
        cam->ts_string = g_strdup(gtk_entry_get_text((GtkEntry *)string_entry));
        g_settings_set_string(cam->gc, CAM_SETTINGS_TIMESTAMP_STRING,
                              cam->ts_string);
    }
    if (cam->debug == TRUE) {
        fprintf(stderr, "dir now = %s\nfile now = %s\n", cam->pixdir,
                cam->capturefile);
    }
    gtk_widget_hide(prefswindow);
}

gboolean delete_event_prefs_window(GtkWidget *widget, GdkEvent *,
                                   cam_t *cam)
{
    prefs_func(widget, cam);
    return TRUE;
}

void on_quit_activate(GtkWidget *, cam_t *cam)
{
    gtk_window_close(GTK_WINDOW(gtk_builder_get_object(cam->xml,
                                                       "main_window")));
}

void on_preferences1_activate(GtkWidget *, gpointer)
{
    gtk_widget_show(prefswindow);
}

void gtk_common_update_image_scale(cam_t *cam, int width, int height)
{
    gchar *title;
    float scale;

    if (cam->scale > 0) {
	scale = 1. * width / cam->width;
	if (1.f * height / cam->height < scale)
	    scale = 1. * height / cam->height;

	cam->scale = scale;
    } else {
	scale = 1.f;
    }

    if (scale == 1.f)
        title = g_strdup_printf("Camorama - %s - %dx%d", cam->name,
                                cam->width, cam->height);
    else
        title = g_strdup_printf("Camorama - %s - %dx%d (scale: %d%%)", cam->name,
                                cam->width, cam->height, (int)(scale * 100.f));
    gtk_window_set_title(GTK_WINDOW(GTK_WIDGET(gtk_builder_get_object(cam->xml, "main_window"))),
                         title);
    g_free(title);
}

gboolean on_configure_event(GtkWidget *, GdkEvent *, cam_t *cam)
{
    GtkWidget *da = GTK_WIDGET(gtk_builder_get_object(cam->xml, "da"));

    gtk_common_update_image_scale(cam, gtk_widget_get_allocated_width(da),
                                  gtk_widget_get_allocated_height(da));

    return FALSE;
}

void gtk_common_show_fullscreen_ui(cam_t *cam, gboolean fullscreen)
{
    if (fullscreen) {
        gtk_widget_hide(GTK_WIDGET(gtk_builder_get_object(cam->xml, "menuitem3")));
        gtk_widget_hide(GTK_WIDGET(gtk_builder_get_object(cam->xml, "menuitem4")));
        gtk_widget_hide(GTK_WIDGET(gtk_builder_get_object(cam->xml, "hbox31")));
        gtk_widget_hide(cam->status);
    } else {
        gtk_widget_show(GTK_WIDGET(gtk_builder_get_object(cam->xml, "menuitem3")));
        gtk_widget_show(GTK_WIDGET(gtk_builder_get_object(cam->xml, "menuitem4")));
        gtk_widget_show(GTK_WIDGET(gtk_builder_get_object(cam->xml, "hbox31")));
        gtk_widget_show(cam->status);
    }
}

void toggle_fullscreen(GtkWidget *, cam_t *cam)
{
    GtkWindow *window = GTK_WINDOW(gtk_builder_get_object(cam->xml,
                                                          "main_window"));
    gboolean is_fullscreen;

#if GTK_MAJOR_VERSION < 4
    is_fullscreen = gtk3_window_is_fullscreen(window);
#else
    is_fullscreen = gtk_window_is_fullscreen(window);
#endif

    if (is_fullscreen) {
        gtk_window_unfullscreen(window);
    } else {
        gtk_window_fullscreen(window);
    }
}


void set_image_scale(cam_t *cam)
{
    GtkWidget *da = GTK_WIDGET(gtk_builder_get_object(cam->xml, "da"));

    if (cam->scale <= 0) {
        GtkWidget *window = GTK_WIDGET(gtk_builder_get_object(cam->xml,
                                                               "main_window"));
        gint window_width = gtk_widget_get_allocated_width(window);
        gint window_height = gtk_widget_get_allocated_height(window);
        gint preview_width = gtk_widget_get_allocated_width(da);
        gint preview_height = gtk_widget_get_allocated_height(da);

        window_width += cam->width - preview_width;
        window_height += cam->height - preview_height;

        gtk_widget_set_hexpand(da, FALSE);
        gtk_widget_set_vexpand(da, FALSE);
        gtk_widget_set_size_request(da, cam->width, cam->height);
#if GTK_MAJOR_VERSION > 3
        gtk_window_set_default_size(GTK_WINDOW(window), window_width,
                                    window_height);
#else
        gtk_window_resize(GTK_WINDOW(window), window_width, window_height);
#endif
    }

    on_configure_event(NULL, NULL, cam);
    gtk_widget_queue_draw(da);

    g_settings_set_int(cam->gc, CAM_SETTINGS_WIDTH, cam->width);
    g_settings_set_int(cam->gc, CAM_SETTINGS_HEIGHT, cam->height);
}

void on_change_size_activate(GtkWidget *widget, cam_t *cam)
{
    gchar const *name;
    unsigned int width = 0, height = 0;

    name = gtk_widget_get_name(widget);
    gtk_popover_popdown(GTK_POPOVER(gtk_builder_get_object(cam->xml,
                                                           "menuitem4_menu")));

    if (strcmp(name, "small") == 0) {
        width = cam->min_width;
        height = cam->min_height;
    } else if (strcmp(name, "medium") == 0) {
        width = cam->max_width / 2;
        height = cam->max_height / 2;
    } else if (strcmp(name, "large") == 0) {
        width = cam->max_width;
        height = cam->max_height;
    } else {
        sscanf(name, "%dx%d", &width, &height);
    }

    try_set_win_info(cam, cam->pixformat, &width, &height);

    /* Nothing to do, so just return */
    if (width == cam->width && height == cam->height)
        return;

    cam->width = width;
    cam->height = height;

    if (cam->debug == TRUE)
        printf("name = %s\n", name);

    if (cam->read == FALSE) {
        if (cam->userptr)
            stop_streaming_userptr(cam);
        else if (cam->read == FALSE)
            stop_streaming(cam);
    }

    set_win_info(cam);

    if (cam->read == FALSE) {
        if (cam->userptr)
            start_streaming_userptr(cam);
        else if (cam->read == FALSE)
            start_streaming(cam);
    }

    set_image_scale(cam);
}

void on_show_adjustments_activate(GtkWidget *button, cam_t *cam)
{
    GtkWidget *menu_button;
    GtkWidget *toolbar_button;
    GtkWidget *other_button;
    gboolean other_active;
    gboolean active;

    g_object_get(button, "active", &active, NULL);

    menu_button = GTK_WIDGET(gtk_builder_get_object(cam->xml,
                                                     "showadjustment_item"));
    toolbar_button = GTK_WIDGET(gtk_builder_get_object(cam->xml,
                                                        "togglebutton1"));
    other_button = button == menu_button ? toolbar_button : menu_button;

    if (other_button) {
        g_object_get(other_button, "active", &other_active, NULL);
        if (other_active != active) {
            g_signal_handlers_block_by_func(other_button,
                                            on_show_adjustments_activate,
                                            cam);
            g_object_set(other_button, "active", active, NULL);
            g_signal_handlers_unblock_by_func(other_button,
                                              on_show_adjustments_activate,
                                              cam);
        }
    }

    if (active) {
        gtk_widget_show(GTK_WIDGET(gtk_builder_get_object(cam->xml, "adjustments_table")));
        cam->show_adjustments = TRUE;
    } else {
        gtk_widget_hide(GTK_WIDGET(gtk_builder_get_object(cam->xml, "adjustments_table")));
        cam->show_adjustments = FALSE;
    }
    g_settings_set_boolean(cam->gc, CAM_SETTINGS_SHOW_ADJUSTMENTS,
                           cam->show_adjustments);
}

void on_show_effects_activate(GtkWidget *button, cam_t *cam)
{
    GtkWidget *effects = GTK_WIDGET(gtk_builder_get_object(cam->xml,
                                                           "scrolledwindow_effects"));
    gboolean active;

    g_object_get(button, "active", &active, NULL);
    cam->show_effects = active;

    if (!cam->show_effects) {
        gtk_widget_hide(effects);
    } else {
        gtk_widget_show(effects);
    }

    g_settings_set_boolean(cam->gc, CAM_SETTINGS_SHOW_EFFECTS, cam->show_effects);
}

static void about_widget_destroy(GtkWidget *)
{
    gtk_widget_destroy(about);
    about = NULL;
}


void on_about_activate(GtkWidget *, cam_t *cam)
{
    const gchar *authors[] = {
        "Greg Jones  <greg@fixedgear.org>",
        "Jens Knutson  <tempest@magusbooks.com>",
        NULL
    };
    const gchar *comments = _("View, alter and save images from a webcam");
    const gchar *translators = _("translator_credits");
    GdkPixbuf *logo = gdk_pixbuf_new_from_file(PACKAGE_DATA_DIR
                                               "/pixmaps/camorama.png",
                                               NULL);

    if (!strcmp(translators, "translator_credits"))
        translators = NULL;
    if (about != NULL) {
        gtk_window_present(GTK_WINDOW(about));
        return;
    }
    about = g_object_new(GTK_TYPE_ABOUT_DIALOG,
                         "name", "Camorama",
                         "version", PACKAGE_VERSION,
                         "copyright", "Copyright \xc2\xa9 2002 Greg Jones",
                         "comments", comments,
                         "authors", authors,
                         "translator-credits", translators,
                         "logo", logo,
                         "license", GPL_LICENSE,
                         "wrap-license", TRUE,
                         NULL);
    gtk_window_set_transient_for(GTK_WINDOW(about),
                                 GTK_WINDOW(GTK_WIDGET(gtk_builder_get_object(cam->xml, "main_window"))));
    g_signal_connect(about, "response",
                     G_CALLBACK(about_widget_destroy), NULL);
    gtk_widget_show(about);
}

static void apply_filters(cam_t *cam, unsigned char *pic_buf)
{
    /* cam_read() always returns RGB24 data in pic_buf. */
    camorama_filter_chain_apply(cam->filter_chain, pic_buf,
                                cam->width, cam->height, 3);
}

#define MULT(d, c, a, t) G_STMT_START { t = c * a + 0x7f; d = ((t >> 8) + t) >> 8; } G_STMT_END

static inline void show_buffer(cam_t *cam)
{
    gtk_widget_queue_draw(GTK_WIDGET(gtk_builder_get_object(cam->xml, "da")));
}

/*
* get image from cam - does all the work ;)
*/
gint timeout_func(cam_t *cam)
{
    unsigned char *pic_buf = cam_read(cam);

    if (!pic_buf)
        return TRUE;

    g_mutex_lock(&cam->pixbuf_mutex);
    apply_filters(cam, pic_buf);

    cam->pb = gdk_pixbuf_new_from_data(pic_buf, GDK_COLORSPACE_RGB, FALSE, 8,
                                       cam->width, cam->height,
                                       (cam->width * 3),
                                       NULL, NULL);
    g_mutex_unlock(&cam->pixbuf_mutex);

    show_buffer(cam);

    return TRUE;
}

gint fps(GtkWidget *sb)
{
    gchar *stat;
    guint cont = gtk_statusbar_get_context_id(GTK_STATUSBAR(sb), "context");

    seconds++;
    stat = g_strdup_printf(_("%.2f fps - current     %.2f fps - average"),
                           frames / 2., frames2 / (seconds * 2.));
    frames = 0;
    gtk_statusbar_push(GTK_STATUSBAR(sb), cont, stat);
    g_free(stat);
    return 1;
}

void on_status_show(GtkWidget *sb, cam_t *cam)
{
    cam->status = sb;
}

void capture_func(GtkWidget *, cam_t *cam)
{
    if (cam->debug == TRUE)
        printf("capture_func\nx = %d, y = %d, depth = %d, realloc size = %d\n",
               cam->width, cam->height, cam->bpp,
               (cam->width * cam->height * cam->bpp / 8));

    if (cam->rcap == TRUE)
        remote_save(cam);

    if (cam->cap == TRUE)
        local_save(cam);
}

gint timeout_capture_func(cam_t *cam)
{
    /* need to return true, or the timeout will be destroyed - don't forget! :) */
    if (cam->hidden == TRUE) {
        /*
         * call timeout_func to get a new picture.   stupid, but it works.
         * also need to add this to capture_func
         * maybe add a "window_state_event" handler to do the same when
         * window is iconified
         */
        timeout_func(cam);
        timeout_func(cam);
        timeout_func(cam);
        timeout_func(cam);

    }

    if (cam->cap == TRUE)
        local_save(cam);

    if (cam->rcap == TRUE)
        remote_save(cam);

    return 1;
}

void gtk_common_update_slider_value(video_controls_t *ctrl, cam_t *cam,
                                    gint32 value)
{
    switch(ctrl->id) {
    case V4L2_CID_CONTRAST:
        gtk_range_set_value((GtkRange *)GTK_WIDGET(gtk_builder_get_object(cam->xml, "contrast_slider")),
                            value);
        break;
    case V4L2_CID_BRIGHTNESS:
        gtk_range_set_value((GtkRange *)GTK_WIDGET(gtk_builder_get_object(cam->xml, "brightness_slider")),
                            value);
        break;
    case V4L2_CID_SATURATION:
        gtk_range_set_value((GtkRange *)GTK_WIDGET(gtk_builder_get_object(cam->xml, "color_slider")),
                            value);
        break;
    case V4L2_CID_ZOOM_ABSOLUTE:
    case V4L2_CID_ZOOM_RELATIVE:
    case V4L2_CID_ZOOM_CONTINUOUS:
        if (cam->zoom_cid != ctrl->id)
            break;
        gtk_range_set_value((GtkRange *)GTK_WIDGET(gtk_builder_get_object(cam->xml, "zoom_slider")),
                            value);
        break;
    case V4L2_CID_HUE:
        gtk_range_set_value((GtkRange *)GTK_WIDGET(gtk_builder_get_object(cam->xml, "hue_slider")),
                            value);
        break;
    case V4L2_CID_WHITENESS:
        gtk_range_set_value((GtkRange *)GTK_WIDGET(gtk_builder_get_object(cam->xml, "balance_slider")),
                            value);
        break;
    default:
        break;
    }
}

static void update_ctrl_value(GtkScale *sc1, video_controls_t *ctrl)
{
    cam_t *cam = ctrl->cam;
    gint32 value;
    int ret;

    ret = cam_get_control(cam, ctrl->id, &value);
    if (ret)
        return;

    gtk_range_set_value((GtkRange *) sc1, value);
    gtk_common_update_slider_value(ctrl, cam, value);
}

static void change_ctrl_value(GtkScale *sc1, video_controls_t *ctrl)
{
    cam_t *cam = ctrl->cam;
    gint32 value;
    int ret;

    value = gtk_range_get_value((GtkRange *) sc1);
    ret = cam_set_control(cam, ctrl->id, &value);
    if (ret) {
        ret = cam_get_control(cam, ctrl->id, &value);
        if (ret)
            return;
        gtk_range_set_value((GtkRange *) sc1, value);
    }
    gtk_common_update_slider_value(ctrl, cam, value);
}

static void update_controls_window(cam_t *cam)
{
    GtkWidget *window = NULL;

    g_mutex_lock(&cam->control_win_mutex);
    if (cam->controls_window)
        window = g_object_ref(cam->controls_window);
    g_mutex_unlock(&cam->control_win_mutex);

    if (window) {
#if GTK_MAJOR_VERSION < 4
        gtk3_update_controls_window(window);
#else
        gtk4_update_controls_window(window);
#endif
        g_object_unref(window);
    }
}

static void reset_ctrls(GtkButton *, cam_t *cam)
{
    video_controls_t *ctrl = cam->controls;
    gint32 value;

    while (ctrl) {
        #pragma GCC diagnostic push
        #pragma GCC diagnostic ignored "-Wswitch-enum"
        switch (ctrl->type) {
        case V4L2_CTRL_TYPE_INTEGER:
        case V4L2_CTRL_TYPE_BOOLEAN:
        case V4L2_CTRL_TYPE_BUTTON:
        case V4L2_CTRL_TYPE_INTEGER_MENU:
        case V4L2_CTRL_TYPE_MENU:
            value = ctrl->def;
            cam_set_control(cam, ctrl->id, &value);
        default:
            break;
        }
        #pragma GCC diagnostic pop

        ctrl = ctrl->next;
    }

    update_controls_window(cam);
}

void gtk_common_clear_controls_window(cam_t *cam)
{
    g_mutex_lock(&cam->control_win_mutex);
    cam->controls_window = NULL;
    g_mutex_unlock(&cam->control_win_mutex);
}

void show_controls(GtkWidget *, cam_t *cam)
{
    GtkWidget *window, *vbox, *grid, *control, *slider, *label, *btn;
    video_controls_t *ctrl;
    char *last_group = NULL;
    gint32 value;
    int row = 0;
    int ret;

    vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 2);
#if GTK_MAJOR_VERSION < 4
    window = gtk3_create_controls_window(vbox, cam);
#else
    window = gtk4_create_controls_window(vbox, cam);
#endif
    gtk_window_set_title(GTK_WINDOW(window), "Camera controls");
    gtk_window_set_default_size(GTK_WINDOW(window), 80, 60);

    if (!g_signal_lookup("control_update", GTK_TYPE_WIDGET))
        g_signal_new("control_update", GTK_TYPE_WIDGET,
                     G_SIGNAL_RUN_LAST, 0, NULL, NULL,
                     g_cclosure_marshal_VOID__VOID, G_TYPE_NONE, 0);

    grid = gtk_grid_new();
#if GTK_MAJOR_VERSION < 4
    gtk3_controls_box_append(GTK_BOX(vbox), grid);
#else
    gtk4_controls_box_append(GTK_BOX(vbox), grid);
#endif

    #pragma GCC diagnostic push
    #pragma GCC diagnostic ignored "-Wswitch-enum"

    ctrl = cam->controls;
    while (ctrl) {
        if (!last_group || strcmp(last_group, ctrl->group)) {
            if (last_group) {
                label = gtk_label_new("");
                gtk_grid_attach(GTK_GRID(grid), label, 0, row++, 2, 1);
            }
            label = gtk_label_new(ctrl->group);
            gtk_grid_attach(GTK_GRID(grid), label, 0, row++, 2, 1);

            last_group = ctrl->group;
        }
        switch (ctrl->type) {
        case V4L2_CTRL_TYPE_INTEGER:
            ret = cam_get_control(cam, ctrl->id, &value);
            if (ret)
                break;

            if (ctrl->min == 0 && ctrl->max == 1) {
                #if GTK_MAJOR_VERSION < 4
                control = gtk3_create_control_button(ctrl, value);
                #else
                control = gtk4_create_control_button(ctrl, value);
                #endif
                gtk_grid_attach(GTK_GRID(grid), control, 1, row++, 1, 1);
                break;
            }

            label = gtk_label_new(ctrl->name);
            gtk_grid_attach(GTK_GRID(grid), label, 0, row, 1, 1);

            slider = gtk_scale_new_with_range(GTK_ORIENTATION_HORIZONTAL,
                                              ctrl->min, ctrl->max, ctrl->step);

            gtk_range_set_value((GtkRange *)slider, value);
            gtk_grid_attach (GTK_GRID(grid), slider, 1, row++, 1, 1);

            g_signal_connect(slider, "value-changed",
                             G_CALLBACK(change_ctrl_value), ctrl);
            g_signal_connect(slider, "control_update",
                             G_CALLBACK(update_ctrl_value), ctrl);
            break;
        case V4L2_CTRL_TYPE_BOOLEAN:
        case V4L2_CTRL_TYPE_BUTTON:
            ret = cam_get_control(cam, ctrl->id, &value);
            if (ret)
                break;

            #if GTK_MAJOR_VERSION < 4
            control = gtk3_create_control_button(ctrl, value);
            #else
            control = gtk4_create_control_button(ctrl, value);
            #endif
            gtk_grid_attach(GTK_GRID(grid), control, 1, row++, 1, 1);
            break;
        case V4L2_CTRL_TYPE_INTEGER_MENU:
        case V4L2_CTRL_TYPE_MENU:
            ret = cam_get_control(cam, ctrl->id, &value);
            if (ret)
                break;

            label = gtk_label_new(ctrl->name);
            gtk_grid_attach(GTK_GRID(grid), label, 0, row, 1, 1);

            #if GTK_MAJOR_VERSION < 4
            control = gtk3_create_control_menu(ctrl, value);
            #else
            control = gtk4_create_control_menu(ctrl, value);
            #endif
            gtk_grid_attach(GTK_GRID(grid), control, 1, row++, 1, 1);
            break;
        default:
            break;
        }
        ctrl = ctrl->next;
    }
    #pragma GCC diagnostic pop

    btn = gtk_button_new_with_label("Reset to default");
#if GTK_MAJOR_VERSION < 4
    gtk3_controls_box_append(GTK_BOX(vbox), btn);
#else
    gtk4_controls_box_append(GTK_BOX(vbox), btn);
#endif
    g_signal_connect(btn, "clicked", G_CALLBACK(reset_ctrls), cam);

    g_mutex_lock(&cam->control_win_mutex);
    cam->controls_window = window;
    g_mutex_unlock(&cam->control_win_mutex);

#if GTK_MAJOR_VERSION < 4
    gtk3_present_controls_window(GTK_WINDOW(window));
#else
    gtk4_present_controls_window(GTK_WINDOW(window));
#endif
}


void update_sliders(cam_t *cam)
{
   video_controls_t *p;

    p = cam_find_control_per_id(cam, V4L2_CID_CONTRAST);
    if (!p) {
        gtk_widget_hide(GTK_WIDGET(gtk_builder_get_object(cam->xml,
                                                          "contrast_icon")));
        gtk_widget_hide(GTK_WIDGET(gtk_builder_get_object(cam->xml,
                                                          "contrast_label")));
        gtk_widget_hide(GTK_WIDGET(gtk_builder_get_object(cam->xml,
                                                          "contrast_slider")));
    } else {
        g_signal_connect(gtk_builder_get_object(cam->xml, "contrast_slider"),
                         "value-changed", G_CALLBACK(contrast_change), cam);

        gtk_range_set_range((GtkRange *)GTK_WIDGET(gtk_builder_get_object(cam->xml, "contrast_slider")),
                            p->min, p->max);
        gtk_range_set_increments((GtkRange *)GTK_WIDGET(gtk_builder_get_object(cam->xml, "contrast_slider")),
                                 p->step, p->step * 5);
        gtk_range_set_value((GtkRange *)GTK_WIDGET(gtk_builder_get_object(cam->xml, "contrast_slider")),
                            cam->contrast);
        gtk_widget_show(GTK_WIDGET(gtk_builder_get_object(cam->xml,
                                                          "contrast_icon")));
        gtk_widget_show(GTK_WIDGET(gtk_builder_get_object(cam->xml,
                                                          "contrast_label")));
        gtk_widget_show(GTK_WIDGET(gtk_builder_get_object(cam->xml,
                                                          "contrast_slider")));
    }

    p = cam_find_control_per_id(cam, V4L2_CID_BRIGHTNESS);
    if (!p) {
        gtk_widget_hide(GTK_WIDGET(gtk_builder_get_object(cam->xml,
                                                          "brightness_icon")));
        gtk_widget_hide(GTK_WIDGET(gtk_builder_get_object(cam->xml,
                                                          "brightness_label")));
        gtk_widget_hide(GTK_WIDGET(gtk_builder_get_object(cam->xml,
                                                          "brightness_slider")));
    } else {
        g_signal_connect(gtk_builder_get_object(cam->xml, "brightness_slider"),
                         "value-changed", G_CALLBACK(brightness_change), cam);
        gtk_range_set_range((GtkRange *)GTK_WIDGET(gtk_builder_get_object(cam->xml, "brightness_slider")),
                            p->min, p->max);
        gtk_range_set_increments((GtkRange *)GTK_WIDGET(gtk_builder_get_object(cam->xml, "brightness_slider")),
                                 p->step, p->step * 5);
        gtk_range_set_value((GtkRange *)GTK_WIDGET(gtk_builder_get_object(cam->xml, "brightness_slider")),
                            cam->brightness);
        gtk_widget_show(GTK_WIDGET(gtk_builder_get_object(cam->xml,
                                                          "brightness_icon")));
        gtk_widget_show(GTK_WIDGET(gtk_builder_get_object(cam->xml,
                                                          "brightness_label")));
        gtk_widget_show(GTK_WIDGET(gtk_builder_get_object(cam->xml,
                                                          "brightness_slider")));
    }

    p = cam_find_control_per_id(cam, V4L2_CID_SATURATION);
    if (!p) {
        gtk_widget_hide(GTK_WIDGET(gtk_builder_get_object(cam->xml,
                                                          "color_icon")));
        gtk_widget_hide(GTK_WIDGET(gtk_builder_get_object(cam->xml,
                                                          "color_label")));
        gtk_widget_hide(GTK_WIDGET(gtk_builder_get_object(cam->xml,
                                                          "color_slider")));
    } else {
        g_signal_connect(gtk_builder_get_object(cam->xml, "color_slider"),
                         "value-changed", G_CALLBACK(colour_change), cam);
        gtk_range_set_range((GtkRange *)GTK_WIDGET(gtk_builder_get_object(cam->xml, "color_slider")),
                            p->min, p->max);
        gtk_range_set_increments((GtkRange *)GTK_WIDGET(gtk_builder_get_object(cam->xml, "color_slider")),
                                 p->step, p->step * 5);
        gtk_range_set_value((GtkRange *)GTK_WIDGET(gtk_builder_get_object(cam->xml, "color_slider")),
                            cam->colour);
        gtk_widget_show(GTK_WIDGET(gtk_builder_get_object(cam->xml,
                                                          "color_icon")));
        gtk_widget_show(GTK_WIDGET(gtk_builder_get_object(cam->xml,
                                                          "color_label")));
        gtk_widget_show(GTK_WIDGET(gtk_builder_get_object(cam->xml,
                                                          "color_slider")));
    }

    p = cam_find_control_per_id(cam, cam->zoom_cid);
    if (!p) {
        gtk_widget_hide(GTK_WIDGET(gtk_builder_get_object(cam->xml,
                                                          "zoom_icon")));
        gtk_widget_hide(GTK_WIDGET(gtk_builder_get_object(cam->xml,
                                                          "zoom_label")));
        gtk_widget_hide(GTK_WIDGET(gtk_builder_get_object(cam->xml,
                                                          "zoom_slider")));
    } else {
        g_signal_connect(gtk_builder_get_object(cam->xml, "zoom_slider"),
                         "value-changed", G_CALLBACK(zoom_change), cam);
        gtk_range_set_range((GtkRange *)GTK_WIDGET(gtk_builder_get_object(cam->xml, "zoom_slider")),
                            p->min, p->max);
        gtk_range_set_increments((GtkRange *)GTK_WIDGET(gtk_builder_get_object(cam->xml, "zoom_slider")),
                                 p->step, p->step * 5);
        gtk_range_set_value((GtkRange *)GTK_WIDGET(gtk_builder_get_object(cam->xml, "zoom_slider")),
                            cam->zoom);
        gtk_widget_show(GTK_WIDGET(gtk_builder_get_object(cam->xml,
                                                          "zoom_icon")));
        gtk_widget_show(GTK_WIDGET(gtk_builder_get_object(cam->xml,
                                                          "zoom_label")));
        gtk_widget_show(GTK_WIDGET(gtk_builder_get_object(cam->xml,
                                                          "zoom_slider")));
    }

    p = cam_find_control_per_id(cam, V4L2_CID_HUE);
    if (!p) {
        gtk_widget_hide(GTK_WIDGET(gtk_builder_get_object(cam->xml,
                                                          "hue_icon")));
        gtk_widget_hide(GTK_WIDGET(gtk_builder_get_object(cam->xml,
                                                          "hue_label")));
        gtk_widget_hide(GTK_WIDGET(gtk_builder_get_object(cam->xml,
                                                          "hue_slider")));
    } else {
        g_signal_connect(gtk_builder_get_object(cam->xml, "hue_slider"),
                         "value-changed", G_CALLBACK(hue_change), cam);
        gtk_range_set_range((GtkRange *)GTK_WIDGET(gtk_builder_get_object(cam->xml, "hue_slider")),
                            p->min, p->max);
        gtk_range_set_increments((GtkRange *)GTK_WIDGET(gtk_builder_get_object(cam->xml, "hue_slider")),
                                 p->step, p->step * 5);
        gtk_range_set_value((GtkRange *)GTK_WIDGET(gtk_builder_get_object(cam->xml, "hue_slider")),
                            cam->hue);
        gtk_widget_show(GTK_WIDGET(gtk_builder_get_object(cam->xml,
                                                          "hue_icon")));
        gtk_widget_show(GTK_WIDGET(gtk_builder_get_object(cam->xml,
                                                          "hue_label")));
        gtk_widget_show(GTK_WIDGET(gtk_builder_get_object(cam->xml,
                                                          "hue_slider")));
    }

    p = cam_find_control_per_id(cam, V4L2_CID_WHITENESS);
    if (!p) {
        gtk_widget_hide(GTK_WIDGET(gtk_builder_get_object(cam->xml,
                                                          "balance_icon")));
        gtk_widget_hide(GTK_WIDGET(gtk_builder_get_object(cam->xml,
                                                          "balance_label")));
        gtk_widget_hide(GTK_WIDGET(gtk_builder_get_object(cam->xml,
                                                          "balance_slider")));
    } else {
        g_signal_connect(gtk_builder_get_object(cam->xml, "balance_slider"),
                         "value-changed", G_CALLBACK(wb_change), cam);
        gtk_range_set_range((GtkRange *)GTK_WIDGET(gtk_builder_get_object(cam->xml, "balance_slider")),
                            p->min, p->max);
        gtk_range_set_increments((GtkRange *)GTK_WIDGET(gtk_builder_get_object(cam->xml, "balance_slider")),
                                 p->step, p->step * 5);
        gtk_range_set_value((GtkRange *)GTK_WIDGET(gtk_builder_get_object(cam->xml, "balance_slider")),
                            cam->whiteness);
        gtk_widget_show(GTK_WIDGET(gtk_builder_get_object(cam->xml,
                                                          "balance_icon")));
        gtk_widget_show(GTK_WIDGET(gtk_builder_get_object(cam->xml,
                                                          "balance_label")));
        gtk_widget_show(GTK_WIDGET(gtk_builder_get_object(cam->xml,
                                                          "balance_slider")));
    }
}

void contrast_change(GtkScale *sc1, cam_t *cam)
{

    cam->contrast = gtk_range_get_value((GtkRange *) sc1);
    cam_set_control(cam, V4L2_CID_CONTRAST, &cam->contrast);

    update_controls_window(cam);
}

void brightness_change(GtkScale *sc1, cam_t *cam)
{

    cam->brightness = gtk_range_get_value((GtkRange *) sc1);
    cam_set_control(cam, V4L2_CID_BRIGHTNESS, &cam->brightness);

    update_controls_window(cam);
}

void zoom_change(GtkScale *sc1, cam_t *cam)
{

    cam->zoom = gtk_range_get_value((GtkRange *) sc1);
    cam_set_control(cam, cam->zoom_cid, &cam->zoom);

    update_controls_window(cam);
}

void colour_change(GtkScale *sc1, cam_t *cam)
{

    cam->colour = gtk_range_get_value((GtkRange *) sc1);
    cam_set_control(cam, V4L2_CID_SATURATION, &cam->colour);

    update_controls_window(cam);
}

void hue_change(GtkScale *sc1, cam_t *cam)
{

    cam->hue = gtk_range_get_value((GtkRange *) sc1);
    cam_set_control(cam, V4L2_CID_HUE, &cam->hue);

    update_controls_window(cam);
}

void wb_change(GtkScale *sc1, cam_t *cam)
{

    cam->whiteness = gtk_range_get_value((GtkRange *) sc1);
    cam_set_control(cam, V4L2_CID_WHITENESS, &cam->whiteness);

    update_controls_window(cam);
}

/*
 * Video device selection
 */

unsigned int n_devices = 0, n_valid_devices = 0;
struct devnodes *devices = NULL;

static int handle_video_devs(const char *file,
                             const struct stat *st,
                             int)
{
    int dev_minor, first_device = -1, fd;
    unsigned int i;
    struct v4l2_capability vid_cap = { 0 };


    /* Discard  devices that can't be a videodev */
    if (!S_ISCHR(st->st_mode) || major(st->st_rdev) != 81)
        return 0;

    dev_minor = minor(st->st_rdev);

    /* check if it is an already existing device */
    if (devices) {
        for (i = 0; i < n_devices; i++) {
            if (dev_minor == devices[i].minor) {
                first_device = i;
                break;
            }
        }
    }

    devices = realloc(devices, (n_devices + 1) * sizeof(struct devnodes));
    if (!devices) {
        char *msg = g_strdup_printf(_("Can't allocate memory to store devices"));
        error_dialog(msg);
        g_free(msg);
        exit(0);
    }
    memset(&devices[n_devices], 0, sizeof(struct devnodes));

    if (first_device < 0) {
        fd = open(file, O_RDWR);
        if (fd < 0) {
            devices[n_devices].is_valid = FALSE;
        } else {
            if (ioctl(fd, VIDIOC_QUERYCAP, &vid_cap) == -1) {
                devices[n_devices].is_valid = FALSE;
            } else if (!(vid_cap.device_caps & V4L2_CAP_VIDEO_CAPTURE)) {
                devices[n_devices].is_valid = FALSE;
            } else {
                n_valid_devices++;
                devices[n_devices].is_valid = TRUE;
            }
        }

        close(fd);
    } else {
        devices[n_devices].is_valid = devices[first_device].is_valid;
    }

    devices[n_devices].fname = g_strdup(file);
    devices[n_devices].minor = dev_minor;
    n_devices++;

    return(0);
}

/*
 * Sort order:
 *
 *  - Valid devices comes first
 *  - Lowest minors comes first
 *
 * For devnode names, it sorts on this order:
 *  - custom udev given names
 *  - /dev/v4l/by-id/
 *  - /dev/v4l/by-path/
 *  - /dev/video
 *  - /dev/char/
 *
 *  - Device name is sorted alphabetically if follows same pattern
 */
static int sort_devices(const void *__a, const void *__b)
{
    const struct devnodes *a = __a;
    const struct devnodes *b = __b;
    int val_a, val_b;

    if (a->is_valid != b->is_valid)
        return !a->is_valid - !b->is_valid;

    if (a->minor != b->minor)
        return a->minor - b->minor;

    /* Ensure that /dev/video* devices will stay at the top */

    if (strstr(a->fname, "by-id"))
        val_a = 1;
    else if (strstr(a->fname, "by-path"))
        val_a = 2;
    else if (strstr(a->fname, "/dev/video"))
        val_a = 3;
    else if (strstr(a->fname, "char"))
        val_a = 4;
    else    /* Customized names comes first */
        val_a = 0;

    if (strstr(b->fname, "by-id"))
        val_b = 1;
    else if (strstr(b->fname, "by-path"))
        val_b = 2;
    else if (strstr(b->fname, "/dev/video"))
        val_b = 3;
    else if (strstr(b->fname, "char"))
        val_b = 4;
    else   /* Customized names comes first */
        val_b = 0;

    if (val_a != val_b)
        return val_a - val_b;

    /* Finally, just use alphabetic order */
    return strcmp(a->fname, b->fname);
}

static void videodev_response(GtkDialog *,
                              cam_t *cam)
{
    GtkWidget *widget;

    widget = GTK_WIDGET(gtk_builder_get_object(cam->xml, "videodev_window"));
    gtk_widget_hide(widget);
}

void retrieve_video_dev(cam_t *cam)
{
    int last_minor = -1;
    unsigned int i;
    GtkWidget *widget;

    /* This function is not meant to be called more than once */
    assert (n_devices == 0);

    /* Get all video devices */
    if (ftw("/dev", handle_video_devs, 4) || !n_devices) {
        char *msg = g_strdup_printf(_("Didn't find any camera"));
        error_dialog(msg);
        g_free(msg);
        exit(0);
    }
    qsort(devices, n_devices, sizeof(struct devnodes), sort_devices);

    if (cam->debug)
        for (i = 0; i < n_devices; i++)
            printf("Device[%d]: %s (%s)\n", i, devices[i].fname,
                   devices[i].is_valid ? "OK" : "NOK");

    /* While we have Gtk 2 support, should be aligned with select_video_dev() */
    widget = GTK_WIDGET(gtk_builder_get_object(cam->xml, "videodev_combo"));
    for (i = 0; i < n_devices; i++) {
        if (devices[i].is_valid) {
            if (devices[i].minor == last_minor)
                continue;
            gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(widget),
                                           devices[i].fname);
            last_minor = devices[i].minor;
        }
    }

    widget = GTK_WIDGET(gtk_builder_get_object(cam->xml, "videodev_ok"));
    g_signal_connect(G_OBJECT(widget), "clicked",
                     G_CALLBACK(videodev_response), cam);
}

int select_video_dev(cam_t *cam)
{
    GtkWidget *window, *widget;
    int ret;

    /* Only ask if there are multiple cameras */
    if (n_valid_devices == 1) {
        cam->video_dev = devices[0].fname;
        return 0;
    }

    /* There are multiple devices. Ask the user */

    window = GTK_WIDGET(gtk_builder_get_object(cam->xml, "videodev_window"));
    widget = GTK_WIDGET(gtk_builder_get_object(cam->xml, "videodev_combo"));

    gtk_combo_box_set_active(GTK_COMBO_BOX(widget), 0);

    gtk_widget_show(window);

    ret = gtk_dialog_run(GTK_DIALOG(window));

    cam->video_dev = gtk_combo_box_text_get_active_text(GTK_COMBO_BOX_TEXT(widget));

    gtk_widget_hide(window);
    return ret;
}

void on_change_camera(GtkWidget *, cam_t *cam)
{
    GtkWidget *window;
    gchar *old_cam;

    old_cam = g_strdup(cam->video_dev);
    select_video_dev(cam);

    /* Trivial case: nothing changed */
    if (!strcmp(cam->video_dev, old_cam)) {
        g_free(old_cam);
        return;
    }
    g_free(old_cam);

    g_mutex_lock(&cam->control_win_mutex);
    if (cam->controls_window) {
        window = cam->controls_window;
        cam->controls_window = NULL;
        g_mutex_unlock(&cam->control_win_mutex);
        gtk_window_close(GTK_WINDOW(window));
    } else {
        g_mutex_unlock(&cam->control_win_mutex);
    }

    start_camera(cam);
    update_sliders(cam);
}

static void add_gtk_view_resolutions(cam_t *cam)
{
    GtkWidget *small_res, *new_res;
    GtkWidget *menu;
    unsigned int i;

    /*
     * Dynamically generate the resolutions based on what the camera
     * actually supports. Provide a fallback method, if the camera driver
     * is too old and doesn't support formats enumeration.
     */

    small_res = GTK_WIDGET(gtk_builder_get_object(cam->xml, "small"));
    menu = GTK_WIDGET(gtk_builder_get_object(cam->xml, "resolution_box"));

    if (!small_res || !GTK_IS_CONTAINER(menu))
        return;

    /* Get all supported resolutions by cam->pixformat */
    get_supported_resolutions(cam, FALSE);

    if (cam->n_res > 0) {
        for (i = 0; i < cam->n_res; i++) {
            char name[80];

            if (cam->res[i].max_fps > 0)
                    sprintf(name, _("%dx%d (max %.1f fps)"),
                            cam->res[i].x, cam->res[i].y,
                            (double)cam->res[i].max_fps);
                else
                    sprintf(name, _("%dx%d"), cam->res[i].x, cam->res[i].y);

            new_res = gtk_button_new_with_label(name);
            gtk_container_add(GTK_CONTAINER(menu), new_res);
            gtk_widget_show(new_res);
            g_signal_connect(new_res, "clicked",
                             G_CALLBACK(on_change_size_activate), cam);
            gtk_widget_set_name(new_res, name);
        }

        /* We won't actually use the small res */
        gtk_widget_hide(small_res);
    } else {
        g_signal_connect(gtk_builder_get_object(cam->xml, "small"),
                         "clicked", G_CALLBACK(on_change_size_activate),
                         cam);

        new_res = gtk_button_new_with_label("Medium");
        gtk_container_add(GTK_CONTAINER(menu), new_res);
        gtk_widget_show(new_res);
        g_signal_connect(new_res, "clicked",
                         G_CALLBACK(on_change_size_activate), cam);
        gtk_widget_set_name(new_res, "medium");

        new_res = gtk_button_new_with_label("Large");
        gtk_container_add(GTK_CONTAINER(menu), new_res);
        gtk_widget_show(new_res);
        g_signal_connect(new_res, "clicked",
                         G_CALLBACK(on_change_size_activate), cam);
        gtk_widget_set_name(new_res, "large");
    }
}

void start_camera(cam_t *cam)
{
    GList *children, *iter;
    GtkWidget *widget, *container;

    /* First step: free used resources, if any */

    if (cam->idle_id) {
        g_source_remove(cam->idle_id);
        cam->idle_id = 0;
    }

    if (cam->dev >= 0) {
        if (cam->read == FALSE) {
            if (cam->userptr)
                stop_streaming_userptr(cam);
            else if (cam->read == FALSE)
                stop_streaming(cam);
        }

        cam->pb = NULL;

        cam_close(cam);
    }

    if (cam->timeout_id) {
        g_source_remove(cam->timeout_id);
        cam->timeout_id = 0;
    }

    if (cam->pic_buf) {
        free(cam->pic_buf);
        cam->pic_buf = NULL;
    }

    if (cam->tmp) {
        free(cam->tmp);
        cam->tmp = NULL;
    }

    cam_free_controls(cam);

    /* Reset also frame counts, as the new camera can use a different fps */
    frames = 0;
    frames2 = 0;
    seconds = 0;

    /* Second step: clean-up all resolutions */

    container = GTK_WIDGET(gtk_builder_get_object(cam->xml, "resolution_box"));
    children = GTK_IS_CONTAINER(container) ?
               gtk_container_get_children(GTK_CONTAINER(container)) : NULL;
    for (iter = children; iter != NULL; iter = g_list_next(iter)) {
        widget = GTK_WIDGET(iter->data);
        if (strstr(gtk_widget_get_name(widget), "x"))
            gtk_widget_destroy(widget);
    }

    /* Third step: allocate them again */

    if (cam->read)
        cam->dev = cam_open(cam, O_RDWR);
    else
        cam->dev = cam_open(cam, O_RDWR | O_NONBLOCK);

    if (camera_cap(cam))
        exit(-1);

    set_win_info(cam);
    get_win_info(cam);

    /* get picture attributes */
    get_pic_info(cam);

    /* Only store the device name after being able to successfully use it */
    g_settings_set_string(cam->gc, CAM_SETTINGS_DEVICE, cam->video_dev);

    /* pic_buf is always for RGB3, which is the format expected by gtk */
    cam->pic_buf = malloc(cam->max_width * cam->max_height * 3);

    if (!cam->pic_buf) {
        printf("Failed to allocate memory for buffers\n");
        exit(0);
    }

    if (cam->read) {
	cam->tmp = malloc(cam->sizeimage);
	if (!cam->pic_buf) {
	    printf("Failed to allocate memory for read buffer\n");
	    exit(0);
	}
    }

    if (cam->read)
        printf("using read()\n");
    else if (cam->userptr)
        start_streaming_userptr(cam);
    else if (cam->read == FALSE)
        start_streaming(cam);

    cam->idle_id = g_idle_add((GSourceFunc) timeout_func, (gpointer) cam);

    if (cam->debug == TRUE)
        print_cam(cam);

    /* Add resolutions */
    add_gtk_view_resolutions(cam);

    /* Adjust image scale */
    set_image_scale(cam);
}
