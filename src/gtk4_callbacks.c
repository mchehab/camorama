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
 * Helper function to set window icons
 */

void gtk4_set_window_icons(GtkWindow *window, GtkWindow *prefswindow)
{
    gtk_window_set_default_icon_name("camorama");
    gtk_window_set_icon_name(window, "camorama");
    gtk_window_set_icon_name(prefswindow, "camorama");
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

/*
 * Helper functions to support preference widgets
 */

const gchar *gtk4_get_entry_text(GtkWidget *entry)
{
    return gtk_editable_get_text(GTK_EDITABLE(entry));
}

void gtk4_set_entry_text(GtkWidget *entry, const gchar *text)
{
    gtk_editable_set_text(GTK_EDITABLE(entry), text);
}

gchar *gtk4_get_file_chooser_folder(GtkWidget *chooser)
{
    GFile *file = gtk_file_chooser_get_current_folder(
        GTK_FILE_CHOOSER(chooser));
    gchar *folder;

    if (!file)
        return NULL;

    folder = g_file_get_path(file);
    g_object_unref(file);

    return folder;
}

void gtk4_set_file_chooser_folder(GtkWidget *chooser, const gchar *folder)
{
    GFile *file = g_file_new_for_path(folder);

    gtk_file_chooser_set_current_folder(GTK_FILE_CHOOSER(chooser), file,
                                        NULL);
    g_object_unref(file);
}

/*
 * Helper functions to support the camera controls window
 */

static void gtk4_update_ctrl_button(GtkCheckButton *button,
                                    video_controls_t *ctrl)
{
    cam_t *cam = ctrl->cam;
    gint32 value;

    if (cam_get_control(cam, ctrl->id, &value))
        return;

    g_signal_handlers_block_by_func(button, gtk4_change_ctrl_button, ctrl);
    gtk_check_button_set_active(button, value);
    g_signal_handlers_unblock_by_func(button, gtk4_change_ctrl_button, ctrl);
    gtk_common_update_slider_value(ctrl, cam, value);
}

static void gtk4_change_ctrl_button(GtkCheckButton *button,
                                    video_controls_t *ctrl)
{
    cam_t *cam = ctrl->cam;
    gint32 value = gtk_check_button_get_active(button) ? 1 : 0;

    if (cam_set_control(cam, ctrl->id, &value))
        g_signal_emit_by_name(button, "control_update");
    else
        gtk_common_update_slider_value(ctrl, cam, value);
}

static void gtk4_update_ctrl_menu(GtkDropDown *combo, video_controls_t *ctrl)
{
    gint32 value;
    unsigned int i;

    if (cam_get_control(ctrl->cam, ctrl->id, &value))
        return;

    for (i = 0; i < ctrl->menu_size; i++) {
        if (ctrl->menu[i].value == value) {
            g_signal_handlers_block_by_func(combo, gtk4_change_ctrl_menu,
                                            ctrl);
            gtk_drop_down_set_selected(combo, i);
            g_signal_handlers_unblock_by_func(combo, gtk4_change_ctrl_menu,
                                              ctrl);
            return;
        }
    }
}

static void gtk4_change_ctrl_menu(GtkDropDown *combo, GParamSpec *,
                                  video_controls_t *ctrl)
{
    unsigned int pos = gtk_drop_down_get_selected(combo);
    gint32 value;

    if (pos == GTK_INVALID_LIST_POSITION || pos >= ctrl->menu_size)
        return;

    value = ctrl->menu[pos].value;
    if (cam_set_control(ctrl->cam, ctrl->id, &value))
        g_signal_emit_by_name(combo, "control_update");
}

static void gtk4_send_control_update(GtkWidget *widget)
{
    GtkWidget *child;

    g_signal_emit_by_name(widget, "control_update");

    for (child = gtk_widget_get_first_child(widget); child;
         child = gtk_widget_get_next_sibling(child))
        gtk4_send_control_update(child);
}

static gboolean gtk4_close_controls(GtkWindow *, cam_t *cam)
{
    gtk_common_clear_controls_window(cam);
    return FALSE;
}

GtkWidget *gtk4_create_controls_window(GtkWidget *child, cam_t *cam)
{
    GtkWidget *window = gtk_window_new();

    gtk_window_set_child(GTK_WINDOW(window), child);
    g_signal_connect(window, "close-request",
                     G_CALLBACK(gtk4_close_controls), cam);

    return window;
}

GtkWidget *gtk4_create_control_button(video_controls_t *ctrl, gint32 value)
{
    GtkWidget *button = gtk_check_button_new_with_label(ctrl->name);

    gtk_check_button_set_active(GTK_CHECK_BUTTON(button), value);
    g_signal_connect(button, "toggled",
                     G_CALLBACK(gtk4_change_ctrl_button), ctrl);
    g_signal_connect(button, "control_update",
                     G_CALLBACK(gtk4_update_ctrl_button), ctrl);

    return button;
}

GtkWidget *gtk4_create_control_menu(video_controls_t *ctrl, gint32 value)
{
    GtkStringList *model = gtk_string_list_new(NULL);
    GtkWidget *combo;
    unsigned int i;

    for (i = 0; i < ctrl->menu_size; i++)
        gtk_string_list_append(model, ctrl->menu[i].name);

    combo = gtk_drop_down_new(G_LIST_MODEL(model), NULL);
    g_object_unref(model);
    for (i = 0; i < ctrl->menu_size; i++) {
        if (ctrl->menu[i].value == value) {
            gtk_drop_down_set_selected(GTK_DROP_DOWN(combo), i);
            break;
        }
    }
    g_signal_connect(combo, "notify::selected",
                     G_CALLBACK(gtk4_change_ctrl_menu), ctrl);
    g_signal_connect(combo, "control_update",
                     G_CALLBACK(gtk4_update_ctrl_menu), ctrl);

    return combo;
}

void gtk4_controls_box_append(GtkBox *box, GtkWidget *child)
{
    gtk_box_append(box, child);
}

void gtk4_update_controls_window(GtkWidget *window)
{
    gtk4_send_control_update(window);
}

void gtk4_present_controls_window(GtkWindow *window)
{
    gtk_window_present(window);
}
