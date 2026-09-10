#include "gtk3_callbacks.h"
#include "camorama-globals.h"

#include <config.h>
#include <glib/gi18n.h>

/*
 * Helper functions to support window area resize
 */

gboolean on_window_state_event(GtkWidget *, GdkEventWindowState *event,
                               cam_t *cam)
{
    gtk_common_show_fullscreen_ui(cam, event->new_window_state &
                                 GDK_WINDOW_STATE_FULLSCREEN);

    return GDK_EVENT_PROPAGATE;
}

gboolean gtk3_window_is_fullscreen(GtkWindow *window)
{
    GdkWindowState state;

    state = gdk_window_get_state(gtk_widget_get_window(GTK_WIDGET(window)));

    return state & GDK_WINDOW_STATE_FULLSCREEN;
}

/*
 * Helper function to set window icons
 */

void gtk3_set_window_icons(GtkWindow *window, GtkWindow *prefswindow)
{
    GdkPixbuf *icon;
    const gchar *icon_file = g_getenv("CAMORAMA_ICON_FILE");

    if (!icon_file)
        icon_file = PACKAGE_DATA_DIR
                    "/icons/hicolor/128x128/devices/camorama.png";

    icon = gdk_pixbuf_new_from_file(icon_file, NULL);
    gtk_window_set_default_icon(icon);
    gtk_window_set_icon(window, icon);
    gtk_window_set_icon(prefswindow, icon);
    g_clear_object(&icon);
}

/*
 * Helper function to support filling the image filling rectangle
 */

gboolean gtk3_draw_frame(GtkWidget *widget, cairo_t *cr, gpointer data)
{
    cam_t *cam = data;
    GdkWindow *window;
    cairo_surface_t *surface;
    const GdkRectangle rect = {
        .x = 0, .y = 0,
        .width = cam->width, .height = cam->height
    };

    if (!cam->pb)
        return GDK_EVENT_PROPAGATE;

    window = gtk_widget_get_window(widget);
    surface = gdk_cairo_surface_create_from_pixbuf(cam->pb, 1, window);

    if (cam->scale > 0 && cam->scale != 1.f)
        cairo_scale(cr, cam->scale, cam->scale);

    cairo_set_source_surface(cr, surface, 0, 0);
    gdk_cairo_rectangle(cr, &rect);
    cairo_fill(cr);
    cairo_surface_destroy(surface);

    frames++;
    frames2++;

    return GDK_EVENT_PROPAGATE;
}

/*
 * Helper functions to support the effects context popup
 */

static gboolean gtk3_effects_button_pressed(GtkTreeView *treeview,
                                            GdkEventButton *event, gpointer)
{
    if (event->button != GDK_BUTTON_SECONDARY)
        return GDK_EVENT_PROPAGATE;

    GTK_WIDGET_GET_CLASS(treeview)->button_press_event(GTK_WIDGET(treeview),
                                                       event);
    gtk_common_show_effects_popup(treeview, event->x, event->y);

    return GDK_EVENT_STOP;
}

static gboolean gtk3_effects_popup_menu(GtkTreeView *treeview, gpointer)
{
    gtk_common_show_effects_popup(treeview, -1, -1);

    return GDK_EVENT_STOP;
}

void gtk3_setup_effects_popup(GtkTreeView *treeview)
{
    g_signal_connect(treeview, "button-press-event",
                     G_CALLBACK(gtk3_effects_button_pressed), NULL);
    g_signal_connect(treeview, "popup-menu",
                     G_CALLBACK(gtk3_effects_popup_menu), NULL);
}

static void gtk3_delete_effects(GtkMenuItem *, GtkTreeView *treeview)
{
    gtk_common_delete_effects(treeview);
}

static void gtk3_add_effect(GtkMenuItem *item, GtkTreeView *treeview)
{
    GType filter_type;

    filter_type = GPOINTER_TO_SIZE(
        g_object_get_data(G_OBJECT(item), "camorama-filter-type"));
    gtk_common_add_effect(treeview, filter_type);
}

static gboolean gtk3_destroy_effects_popup(gpointer menu)
{
    gtk_widget_destroy(menu);

    return G_SOURCE_REMOVE;
}

static void gtk3_effects_popup_deactivated(GtkWidget *menu, gpointer)
{
    g_idle_add_full(G_PRIORITY_DEFAULT_IDLE, gtk3_destroy_effects_popup,
                    g_object_ref(menu), g_object_unref);
}

void gtk3_show_effects_popup(GtkTreeView *treeview, GMenuModel *,
                             GActionGroup *, GPtrArray *entries,
                             double, double)
{
    GtkWidget *menu = gtk_menu_new();
    GtkWidget *add_filters = gtk_menu_new();
    GtkWidget *item;
    guint i;

    gtk_menu_attach_to_widget(GTK_MENU(menu), GTK_WIDGET(treeview), NULL);

    item = gtk_menu_item_new_with_mnemonic("_Delete");
    g_signal_connect(item, "activate",
                     G_CALLBACK(gtk3_delete_effects), treeview);
    gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);
    gtk_menu_shell_append(GTK_MENU_SHELL(menu),
                          gtk_separator_menu_item_new());
    gtk_widget_set_sensitive(
        item, gtk_tree_selection_count_selected_rows(
            gtk_tree_view_get_selection(treeview)) > 0);

    item = gtk_menu_item_new_with_mnemonic(_("_Add Filter"));
    gtk_menu_item_set_submenu(GTK_MENU_ITEM(item), add_filters);
    gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);

    for (i = 0; i < entries->len; i++) {
        effect_menu_entry_t *entry = g_ptr_array_index(entries, i);

        item = gtk_menu_item_new_with_label(entry->name);
        g_object_set_data(G_OBJECT(item), "camorama-filter-type",
                          GSIZE_TO_POINTER(entry->type));
        g_signal_connect(item, "activate",
                         G_CALLBACK(gtk3_add_effect), treeview);
        gtk_menu_shell_append(GTK_MENU_SHELL(add_filters), item);
    }

    g_signal_connect(menu, "deactivate",
                     G_CALLBACK(gtk3_effects_popup_deactivated), NULL);
    gtk_widget_show_all(menu);
    gtk_menu_popup_at_pointer(GTK_MENU(menu), NULL);
}

/*
 * Helper functions to support preference widgets
 */

const gchar *gtk3_get_entry_text(GtkWidget *entry)
{
    return gtk_entry_get_text(GTK_ENTRY(entry));
}

void gtk3_set_entry_text(GtkWidget *entry, const gchar *text)
{
    gtk_entry_set_text(GTK_ENTRY(entry), text);
}

gchar *gtk3_get_file_chooser_folder(GtkWidget *chooser)
{
    return gtk_file_chooser_get_current_folder(GTK_FILE_CHOOSER(chooser));
}

void gtk3_set_file_chooser_folder(GtkWidget *chooser, const gchar *folder)
{
    gtk_file_chooser_set_current_folder(GTK_FILE_CHOOSER(chooser), folder);
}

/*
 * Helper functions to support dialogs
 */

gint gtk3_dialog_run(GtkDialog *dialog)
{
    return gtk_dialog_run(dialog);
}

void gtk3_destroy_widget(GtkWidget *widget)
{
    gtk_widget_destroy(widget);
}

/*
 * Helper functions to support the camera controls window
 */

static void gtk3_update_ctrl_button(GtkToggleButton *button,
                                    video_controls_t *ctrl)
{
    cam_t *cam = ctrl->cam;
    gint32 value;

    if (cam_get_control(cam, ctrl->id, &value))
        return;

    gtk_toggle_button_set_active(button, value);
    gtk_common_update_slider_value(ctrl, cam, value);
}

static void gtk3_change_ctrl_button(GtkToggleButton *button,
                                    video_controls_t *ctrl)
{
    cam_t *cam = ctrl->cam;
    gint32 value = gtk_toggle_button_get_active(button) ? 1 : 0;

    if (cam_set_control(cam, ctrl->id, &value)) {
        if (cam_get_control(cam, ctrl->id, &value))
            return;
        gtk_toggle_button_set_active(button, value);
    }
    gtk_common_update_slider_value(ctrl, cam, value);
}

static void gtk3_update_ctrl_menu(GtkComboBox *combo, video_controls_t *ctrl)
{
    gint32 value;
    unsigned int i;

    if (cam_get_control(ctrl->cam, ctrl->id, &value))
        return;

    for (i = 0; i < ctrl->menu_size; i++) {
        if (ctrl->menu[i].value == value) {
            gtk_combo_box_set_active(combo, i);
            return;
        }
    }
}

static void gtk3_change_ctrl_menu(GtkComboBox *combo, video_controls_t *ctrl)
{
    gint32 value;
    int pos = gtk_combo_box_get_active(combo);

    if (pos < 0)
        return;

    value = ctrl->menu[pos].value;
    if (cam_set_control(ctrl->cam, ctrl->id, &value))
        gtk3_update_ctrl_menu(combo, ctrl);
}

static void gtk3_send_control_update(GtkWidget *widget, gpointer)
{
    g_signal_emit_by_name(widget, "control_update");

    if (GTK_IS_CONTAINER(widget))
        gtk_container_forall(GTK_CONTAINER(widget),
                             gtk3_send_control_update, NULL);
}

static void gtk3_close_controls(GtkWidget *, cam_t *cam)
{
    gtk_common_clear_controls_window(cam);
}

GtkWidget *gtk3_create_controls_window(GtkWidget *child, cam_t *cam)
{
    GtkWidget *window = gtk_window_new(GTK_WINDOW_TOPLEVEL);

    gtk_container_add(GTK_CONTAINER(window), child);
    g_signal_connect(window, "destroy", G_CALLBACK(gtk3_close_controls), cam);

    return window;
}

GtkWidget *gtk3_create_control_button(video_controls_t *ctrl, gint32 value)
{
    GtkWidget *button = gtk_check_button_new_with_label(ctrl->name);

    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(button), value);
    g_signal_connect(button, "clicked",
                     G_CALLBACK(gtk3_change_ctrl_button), ctrl);
    g_signal_connect(button, "control_update",
                     G_CALLBACK(gtk3_update_ctrl_button), ctrl);

    return button;
}

GtkWidget *gtk3_create_control_menu(video_controls_t *ctrl, gint32 value)
{
    GtkWidget *combo = gtk_combo_box_text_new();
    unsigned int i;

    for (i = 0; i < ctrl->menu_size; i++) {
        gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(combo),
                                       ctrl->menu[i].name);
        if (ctrl->menu[i].value == value)
            gtk_combo_box_set_active(GTK_COMBO_BOX(combo), i);
    }
    g_signal_connect(combo, "changed",
                     G_CALLBACK(gtk3_change_ctrl_menu), ctrl);
    g_signal_connect(combo, "control_update",
                     G_CALLBACK(gtk3_update_ctrl_menu), ctrl);

    return combo;
}

void gtk3_controls_box_append(GtkBox *box, GtkWidget *child)
{
    gtk_container_add(GTK_CONTAINER(box), child);
}

void gtk3_update_controls_window(GtkWidget *window)
{
    gtk3_send_control_update(window, NULL);
}

void gtk3_present_controls_window(GtkWindow *window)
{
    gtk_widget_show_all(GTK_WIDGET(window));
}
