#include "gtk4_callbacks.h"
#include "camorama-globals.h"

#include <glib/gi18n.h>

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

/*
 * Helper functions to support the effects context popup
 */

static void gtk4_effects_popup_closed(GtkPopover *popover, gpointer)
{
    gtk_widget_unparent(GTK_WIDGET(popover));
    g_object_unref(popover);
}

static void gtk4_effects_button_pressed(GtkGestureClick *, gint,
                                        gdouble x, gdouble y,
                                        GtkTreeView *treeview)
{
    gtk_common_show_effects_popup(treeview, x, y);
}

static gboolean gtk4_effects_key_pressed(GtkEventControllerKey *, guint keyval,
                                         guint, GdkModifierType state,
                                         GtkTreeView *treeview)
{
    if (keyval != GDK_KEY_Menu &&
        (keyval != GDK_KEY_F10 || !(state & GDK_SHIFT_MASK)))
        return GDK_EVENT_PROPAGATE;

    gtk_common_show_effects_popup(
        treeview, gtk_widget_get_width(GTK_WIDGET(treeview)) / 2,
        gtk_widget_get_height(GTK_WIDGET(treeview)) / 2);

    return GDK_EVENT_STOP;
}

void gtk4_setup_effects_popup(GtkTreeView *treeview)
{
    GtkGesture *gesture = gtk_gesture_click_new();
    GtkEventController *key = gtk_event_controller_key_new();

    gtk_gesture_single_set_button(GTK_GESTURE_SINGLE(gesture),
                                  GDK_BUTTON_SECONDARY);
    g_signal_connect(gesture, "pressed",
                     G_CALLBACK(gtk4_effects_button_pressed), treeview);
    gtk_widget_add_controller(GTK_WIDGET(treeview),
                              GTK_EVENT_CONTROLLER(gesture));

    g_signal_connect(key, "key-pressed",
                     G_CALLBACK(gtk4_effects_key_pressed), treeview);
    gtk_widget_add_controller(GTK_WIDGET(treeview), key);
}

void gtk4_show_effects_popup(GtkTreeView *treeview, GMenuModel *model,
                             GActionGroup *actions, GPtrArray *,
                             double x, double y)
{
    GtkWidget *popover = gtk_popover_menu_new_from_model(model);
    GdkRectangle rect = { x, y, 1, 1 };

    gtk_widget_insert_action_group(popover, "effects", actions);
    gtk_popover_set_pointing_to(GTK_POPOVER(popover), &rect);
    g_object_ref_sink(popover);
    gtk_widget_set_parent(popover, GTK_WIDGET(treeview));
    g_signal_connect(popover, "closed",
                     G_CALLBACK(gtk4_effects_popup_closed), NULL);
    gtk_popover_popup(GTK_POPOVER(popover));
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

static const char file_chooser_folder_key[] =
    "camorama-file-chooser-folder";

#if GTK_CHECK_VERSION(4, 10, 0)

static void gtk4_file_dialog_response(GObject *source_object,
                                      GAsyncResult *result,
                                      gpointer user_data)
{
    GtkFileDialog *dialog = GTK_FILE_DIALOG(source_object);
    GtkButton *button = GTK_BUTTON(user_data);
    GError *error = NULL;
    GFile *file;
    gchar *folder;

    file = gtk_file_dialog_select_folder_finish(dialog, result, &error);
    if (file) {
        folder = g_file_get_path(file);
        if (folder) {
            gtk_button_set_label(button, folder);
            g_object_set_data_full(G_OBJECT(button),
                                   file_chooser_folder_key,
                                   folder, g_free);
        }
        g_object_unref(file);
    }
    g_clear_error(&error);
    g_object_unref(button);
    g_object_unref(dialog);
}

static void gtk4_file_chooser_clicked(GtkButton *button, gpointer)
{
    GtkFileDialog *dialog;
    GtkWindow *parent;
    const gchar *folder;
    GFile *file = NULL;

    dialog = gtk_file_dialog_new();
    gtk_file_dialog_set_title(dialog, _("Select a Directory"));
    gtk_file_dialog_set_accept_label(dialog, _("_Select"));

    folder = g_object_get_data(G_OBJECT(button), file_chooser_folder_key);
    if (folder)
        file = g_file_new_for_path(folder);
    if (file) {
        gtk_file_dialog_set_initial_folder(dialog, file);
        g_object_unref(file);
    }

    parent = GTK_WINDOW(gtk_widget_get_root(GTK_WIDGET(button)));
    g_object_ref(button);
    gtk_file_dialog_select_folder(dialog, parent, NULL,
                                  gtk4_file_dialog_response, button);
}

#else

static void gtk4_file_chooser_response(GtkNativeDialog *dialog,
                                       gint response, GtkButton *button)
{
    GFile *file;
    gchar *folder = NULL;

    if (response == GTK_RESPONSE_ACCEPT) {
        file = gtk_file_chooser_get_file(GTK_FILE_CHOOSER(dialog));
        if (file) {
            folder = g_file_get_path(file);
            g_object_unref(file);
        }
    }

    if (folder) {
        gtk_button_set_label(button, folder);
        g_object_set_data_full(G_OBJECT(button), file_chooser_folder_key,
                               folder, g_free);
    }

    g_object_unref(dialog);
}

static void gtk4_file_chooser_clicked(GtkButton *button, gpointer)
{
    GtkFileChooserNative *dialog;
    const gchar *folder;
    GFile *file;

    dialog = gtk_file_chooser_native_new(
        _("Select a Directory"),
        GTK_WINDOW(gtk_widget_get_root(GTK_WIDGET(button))),
        GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER, _("_Select"), _("_Cancel"));
    folder = g_object_get_data(G_OBJECT(button), file_chooser_folder_key);
    if (folder) {
        file = g_file_new_for_path(folder);
        gtk_file_chooser_set_current_folder(GTK_FILE_CHOOSER(dialog), file,
                                            NULL);
        g_object_unref(file);
    }
    g_signal_connect(dialog, "response",
                     G_CALLBACK(gtk4_file_chooser_response), button);
    gtk_native_dialog_show(GTK_NATIVE_DIALOG(dialog));
}

#endif

gchar *gtk4_get_file_chooser_folder(GtkWidget *chooser)
{
    return g_strdup(g_object_get_data(G_OBJECT(chooser),
                                      file_chooser_folder_key));
}

void gtk4_set_file_chooser_folder(GtkWidget *chooser, const gchar *folder)
{
    gtk_button_set_label(GTK_BUTTON(chooser), folder);
    g_object_set_data_full(G_OBJECT(chooser), file_chooser_folder_key,
                          g_strdup(folder), g_free);
    g_signal_connect(chooser, "clicked",
                     G_CALLBACK(gtk4_file_chooser_clicked), NULL);
}

/*
 * Helper functions to support dialogs
 */

struct gtk4_dialog_run_data {
    GMainLoop *loop;
    gint response;
};

static void gtk4_dialog_response(GtkDialog *, gint response,
                                 struct gtk4_dialog_run_data *data)
{
    data->response = response;
    g_main_loop_quit(data->loop);
}

static gboolean gtk4_dialog_close_requested(
    GtkWindow *, struct gtk4_dialog_run_data *data)
{
    data->response = GTK_RESPONSE_DELETE_EVENT;
    g_main_loop_quit(data->loop);

    return TRUE;
}

gint gtk4_dialog_run(GtkDialog *dialog)
{
    struct gtk4_dialog_run_data data = {
        .loop = g_main_loop_new(NULL, FALSE),
        .response = GTK_RESPONSE_NONE,
    };
    gboolean modal = gtk_window_get_modal(GTK_WINDOW(dialog));
    gulong response_id;
    gulong close_id;

    response_id = g_signal_connect(dialog, "response",
                                   G_CALLBACK(gtk4_dialog_response), &data);
    close_id = g_signal_connect(dialog, "close-request",
                                G_CALLBACK(gtk4_dialog_close_requested),
                                &data);
    gtk_window_set_modal(GTK_WINDOW(dialog), TRUE);
    gtk_window_present(GTK_WINDOW(dialog));
    g_main_loop_run(data.loop);
    gtk_window_set_modal(GTK_WINDOW(dialog), modal);
    g_signal_handler_disconnect(dialog, response_id);
    g_signal_handler_disconnect(dialog, close_id);
    g_main_loop_unref(data.loop);

    return data.response;
}

static void gtk4_window_response(GtkWidget *,
                                 struct gtk4_dialog_run_data *data)
{
    data->response = GTK_RESPONSE_ACCEPT;
    g_main_loop_quit(data->loop);
}

gint gtk4_window_run(GtkWindow *window, GtkWidget *response_widget)
{
    struct gtk4_dialog_run_data data = {
        .loop = g_main_loop_new(NULL, FALSE),
        .response = GTK_RESPONSE_NONE,
    };
    gboolean modal = gtk_window_get_modal(window);
    gulong response_id;
    gulong close_id;

    response_id = g_signal_connect(response_widget, "clicked",
                                   G_CALLBACK(gtk4_window_response), &data);
    close_id = g_signal_connect(window, "close-request",
                                G_CALLBACK(gtk4_dialog_close_requested),
                                &data);
    gtk_window_set_modal(window, TRUE);
    gtk_window_present(window);
    g_main_loop_run(data.loop);
    gtk_window_set_modal(window, modal);
    g_signal_handler_disconnect(response_widget, response_id);
    g_signal_handler_disconnect(window, close_id);
    g_main_loop_unref(data.loop);

    return data.response;
}

int gtk4_error_dialog(const gchar *message)
{
    GtkApplication *app;
    GtkWindow *parent = NULL;
    GtkWidget *window;
    GtkWidget *box;
    GtkWidget *label;
    GtkWidget *button;
    int response;

    app = GTK_APPLICATION(g_application_get_default());
    if (GTK_IS_APPLICATION(app))
        parent = gtk_application_get_active_window(app);

    if (!parent) {
        g_printerr("Camorama: %s\n", message);
        return 0;
    }

    window = gtk_window_new();
    gtk_window_set_title(GTK_WINDOW(window), _("Camorama"));
    gtk_window_set_transient_for(GTK_WINDOW(window), parent);
    gtk_window_set_destroy_with_parent(GTK_WINDOW(window), TRUE);

    box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 12);
    gtk_widget_set_margin_top(box, 12);
    gtk_widget_set_margin_bottom(box, 12);
    gtk_widget_set_margin_start(box, 12);
    gtk_widget_set_margin_end(box, 12);
    gtk_window_set_child(GTK_WINDOW(window), box);

    label = gtk_label_new(message);
    gtk_label_set_wrap(GTK_LABEL(label), TRUE);
    gtk_label_set_selectable(GTK_LABEL(label), TRUE);
    gtk_box_append(GTK_BOX(box), label);

    button = gtk_button_new_with_mnemonic(_("_Close"));
    gtk_widget_set_halign(button, GTK_ALIGN_END);
    gtk_box_append(GTK_BOX(box), button);

    response = gtk4_window_run(GTK_WINDOW(window), button);
    gtk_window_destroy(GTK_WINDOW(window));

    return response;
}

void gtk4_destroy_widget(GtkWidget *widget)
{
    GtkWidget *parent;

    if (GTK_IS_WINDOW(widget)) {
        gtk_window_destroy(GTK_WINDOW(widget));
        return;
    }

    parent = gtk_widget_get_parent(widget);
    if (GTK_IS_BOX(parent))
        gtk_box_remove(GTK_BOX(parent), widget);
    else if (parent)
        gtk_widget_unparent(widget);
}

gboolean gtk4_close_prefs_window(GtkWindow *window, cam_t *cam)
{
    prefs_func(GTK_WIDGET(window), cam);

    return GDK_EVENT_STOP;
}

/*
 * Helper functions to support container operations
 */

void gtk4_box_append(GtkBox *box, GtkWidget *child)
{
    gtk_box_append(box, child);
}

GList *gtk4_get_children(GtkWidget *widget)
{
    GtkWidget *child;
    GList *children = NULL;

    for (child = gtk_widget_get_first_child(widget); child;
         child = gtk_widget_get_next_sibling(child))
        children = g_list_prepend(children, child);

    return g_list_reverse(children);
}

/*
 * Helper functions to support the camera controls window
 */

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

void gtk4_update_controls_window(GtkWidget *window)
{
    gtk4_send_control_update(window);
}

void gtk4_present_controls_window(GtkWindow *window)
{
    gtk_window_present(window);
}
