/* This file is part of camorama
 *
 * AUTHORS
 *     Sven Herzberg  <herzi@gnome-de.org>
 *
 * Copyright (C) 2003  Greg Jones
 * Copyright (C) 2006  Sven Herzberg <herzi@gnome-de.org>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307
 * USA
 */

#include "camorama-window.h"

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#include <glib.h>
#include <glib/gi18n.h>
#if GTK_MAJOR_VERSION < 4
#include "gtk3_callbacks.h"
#else
#include "gtk4_callbacks.h"
#endif
#include "camorama-filter-chain.h"
#include "camorama-globals.h"
#include "filter.h"
#include "support.h"


/* Supported URI protocol schemas */
const gchar *const protos[3] = { "ftp", "sftp", "smb" };

/* Animation delays for filter removal */
#define EFFECT_REMOVE_DELAY 100
#define EFFECT_TRANSITION_DURATION 400

struct effect_info {
    GType type;
    gchar *name;
};

struct effects_pane {
    cam_t *cam;
    GtkListBox *list;
    GPtrArray *effects;
};

struct effect_row {
    struct effects_pane *pane;

    GtkWidget *row;
    GtkRevealer *revealer;
    GArray *choices;
    GType selected_type;

    gulong changed_handler;
    gboolean removing;
    guint animation_source;

#if GTK_MAJOR_VERSION < 4
    GtkComboBoxText *choice;
#else
    GtkDropDown *choice;
    GtkStringList *model;

    gulong revealer_handler;
    gboolean remove_pending;
    gboolean refresh_pending;
    guint refresh_source;
    guint remove_source;
#endif
};

static void append_choice(struct effect_row *effect_row, GType type,
                          const gchar *name)
{
#if GTK_MAJOR_VERSION < 4
    gtk_combo_box_text_append_text(effect_row->choice, name);
#else
    gtk_string_list_append(effect_row->model, name);
#endif
    g_array_append_val(effect_row->choices, type);
}

static void clear_choices(struct effect_row *effect_row)
{
#if GTK_MAJOR_VERSION < 4
    gtk_combo_box_text_remove_all(effect_row->choice);
#else
    guint n_items = g_list_model_get_n_items(G_LIST_MODEL(effect_row->model));

    gtk_string_list_splice(effect_row->model, 0, n_items, NULL);
#endif
    g_array_set_size(effect_row->choices, 0);
}

static void set_choice_active(struct effect_row *effect_row, guint index)
{
#if GTK_MAJOR_VERSION < 4
    gtk_combo_box_set_active(GTK_COMBO_BOX(effect_row->choice), index);
#else
    gtk_drop_down_set_selected(effect_row->choice, index);
#endif
}

static gint get_choice_active(struct effect_row *effect_row)
{
#if GTK_MAJOR_VERSION < 4
    return gtk_combo_box_get_active(GTK_COMBO_BOX(effect_row->choice));
#else
    guint index = gtk_drop_down_get_selected(effect_row->choice);

    return index == GTK_INVALID_LIST_POSITION ? -1 : (gint)index;
#endif
}

static void clear_source(guint *source)
{
    if (*source)
        g_source_remove(*source);

    *source = 0;
}

static void fill_effect_choices(struct effect_row *effect_row,
                                gboolean is_active)
{
    const char *no_effect = _("<No effect>");
    const char *disable = _("<Disable>");
    struct effect_info *effect;
    guint i;

    /* No effects ative - show them in order starting from no_effect */
    if (!is_active) {
        append_choice(effect_row, G_TYPE_INVALID, no_effect);

        for (i = 0; i < effect_row->pane->effects->len; i++) {
            effect = g_ptr_array_index(effect_row->pane->effects, i);

            append_choice(effect_row, effect->type, effect->name);
        }
        return;
    }

    /*
     * The effect in this row is active. It could simply use the code above,
     * but doing a button reorder makes easier to show the current affect and
     * to have the disable choice just after it.
     */

    /* First element: the current effect */
    for (i = 0; i < effect_row->pane->effects->len; i++) {
        effect = g_ptr_array_index(effect_row->pane->effects, i);

        if (effect->type == effect_row->selected_type) {
            append_choice(effect_row, effect->type, effect->name);
            break;
        }
    }

    /* Second element: disable effects */
    append_choice(effect_row, G_TYPE_INVALID, disable);

    /* Add the remaining filters */
    for (i = 0; i < effect_row->pane->effects->len; i++) {
        effect = g_ptr_array_index(effect_row->pane->effects, i);

        if (effect->type != effect_row->selected_type)
            append_choice(effect_row, effect->type, effect->name);
    }

}

static void show_active_effect(struct effect_row *effect_row)
{
    g_signal_handler_block(effect_row->choice,
                           effect_row->changed_handler);

    clear_choices(effect_row);
    fill_effect_choices(effect_row, TRUE);

    set_choice_active(effect_row, 0);
    g_signal_handler_unblock(effect_row->choice,
                             effect_row->changed_handler);
}

#if GTK_MAJOR_VERSION >= 4
static void prepare_effect_row_destroy(struct effect_row *effect_row,
                                       gboolean cancel_remove)
{
    clear_source(&effect_row->animation_source);
    clear_source(&effect_row->refresh_source);

    if (cancel_remove)
        clear_source(&effect_row->remove_source);

    if (effect_row->changed_handler) {
        g_signal_handler_disconnect(effect_row->choice,
                                    effect_row->changed_handler);
        effect_row->changed_handler = 0;
    }

    if (effect_row->revealer_handler) {
        g_signal_handler_disconnect(effect_row->revealer,
                                    effect_row->revealer_handler);
        effect_row->revealer_handler = 0;
    }

    gtk_drop_down_set_model(effect_row->choice, NULL);
    effect_row->model = NULL;
}

static gboolean remove_effect_row_idle(gpointer data)
{
    GtkWidget *row = data;
    GtkWidget *parent = gtk_widget_get_parent(row);
    struct effect_row *effect_row = g_object_get_data(
        G_OBJECT(row), "effect-row");

    if (effect_row)
        effect_row->remove_source = 0;

    if (effect_row)
        prepare_effect_row_destroy(effect_row, FALSE);
    if (GTK_IS_LIST_BOX(parent))
        gtk_list_box_remove(GTK_LIST_BOX(parent), row);

    return G_SOURCE_REMOVE;
}

static gboolean refresh_effect_row(gpointer data)
{
    GtkWidget *row = data;
    struct effect_row *effect_row = g_object_get_data(
        G_OBJECT(row), "effect-row");

    if (effect_row)
        effect_row->refresh_source = 0;

    if (effect_row && gtk_widget_get_parent(row)) {
        effect_row->refresh_pending = FALSE;
        show_active_effect(effect_row);
        gtk_widget_set_sensitive(GTK_WIDGET(effect_row->choice), TRUE);
    }

    return G_SOURCE_REMOVE;
}

static void queue_effect_row_refresh(struct effect_row *effect_row)
{
    effect_row->refresh_pending = TRUE;
    gtk_widget_set_sensitive(GTK_WIDGET(effect_row->choice), FALSE);

    effect_row->refresh_source = g_idle_add_full(G_PRIORITY_DEFAULT_IDLE, refresh_effect_row,
                                                 g_object_ref(effect_row->row),
                                                 g_object_unref);
}
#endif

static void remove_effect_row(GtkRevealer *revealer, GParamSpec *,
                              struct effect_row *effect_row)
{
    if (!effect_row->removing || gtk_revealer_get_child_revealed(revealer))
        return;

#if GTK_MAJOR_VERSION < 4
    gtk_container_remove(GTK_CONTAINER(effect_row->pane->list),
                         effect_row->row);
#else
    if (effect_row->remove_pending)
        return;

    effect_row->remove_pending = TRUE;
    effect_row->remove_source = g_idle_add_full(
        G_PRIORITY_DEFAULT_IDLE, remove_effect_row_idle,
        g_object_ref(effect_row->row), g_object_unref);
#endif
}

static gboolean reveal_effect_row(gpointer data)
{
    GtkWidget *row = data;
    struct effect_row *effect_row = g_object_get_data(
        G_OBJECT(row), "effect-row");

    if (effect_row) {
        effect_row->animation_source = 0;
        gtk_revealer_set_reveal_child(effect_row->revealer, TRUE);
    }

    return G_SOURCE_REMOVE;
}

static gboolean conceal_effect_row(gpointer data)
{
    GtkWidget *row = data;
    struct effect_row *effect_row = g_object_get_data(
        G_OBJECT(row), "effect-row");

    if (effect_row) {
        effect_row->animation_source = 0;
        gtk_revealer_set_reveal_child(effect_row->revealer, FALSE);
    }

    return G_SOURCE_REMOVE;
}

static void append_effect_row(struct effects_pane *pane, gboolean animate);

static void update_effect_row(struct effect_row *effect_row)
{
#if GTK_MAJOR_VERSION < 4
    show_active_effect(effect_row);
#else
    queue_effect_row_refresh(effect_row);
#endif
}

static guint effect_row_position(struct effect_row *effect_row)
{
#if GTK_MAJOR_VERSION < 4
    GList *children;
    GList *item;
#else
    GtkWidget *child;
#endif
    guint position = 0;

#if GTK_MAJOR_VERSION < 4
    children = gtk_container_get_children(GTK_CONTAINER(effect_row->pane->list));

    for (item = children; item; item = item->next) {
        struct effect_row *other = g_object_get_data(G_OBJECT(item->data),
                                                      "effect-row");

        if (other == effect_row)
            break;

        if (other && other->selected_type != G_TYPE_INVALID)
            position++;
    }
    g_list_free(children);
#else
    child = gtk_widget_get_first_child(GTK_WIDGET(effect_row->pane->list));

    for (; child; child = gtk_widget_get_next_sibling(child)) {
        struct effect_row *other = g_object_get_data(G_OBJECT(child),
                                                      "effect-row");

        if (other == effect_row)
            break;

        if (other && other->selected_type != G_TYPE_INVALID)
            position++;
    }
#endif

    return position;
}

static void effect_changed(struct effect_row *effect_row)
{
    gint selected = get_choice_active(effect_row);
    CamoramaFilterChain *chain;
    unsigned int position;
    GType selected_type;

    if (selected < 0 || (guint)selected >= effect_row->choices->len)
        return;

    selected_type = g_array_index(effect_row->choices, GType, selected);
    if (selected_type == effect_row->selected_type)
        return;

    chain = effect_row->pane->cam->filter_chain;
    position = effect_row_position(effect_row);

    if (selected_type == G_TYPE_INVALID) {
        /* Remove effect */
        if (position < chain->filters->len) {
            CamoramaFilter *filter = g_ptr_array_index(chain->filters,
                                                        position);

            camorama_filter_hide(filter);
            g_ptr_array_remove_index(chain->filters, position);
        }

        effect_row->selected_type = G_TYPE_INVALID;
        effect_row->removing = TRUE;
        gtk_widget_set_sensitive(GTK_WIDGET(effect_row->choice), FALSE);
        effect_row->animation_source = g_timeout_add_full(
            G_PRIORITY_DEFAULT, EFFECT_REMOVE_DELAY,
            conceal_effect_row, g_object_ref(effect_row->row),
            g_object_unref);

        return;
    }

    if (effect_row->selected_type == G_TYPE_INVALID) {
        /* Insert effect */
        CamoramaFilter *filter = g_object_new(selected_type, NULL);

        camorama_filter_show(filter, effect_row->pane->cam);
        g_ptr_array_insert(chain->filters, position, filter);

        effect_row->selected_type = selected_type;
        update_effect_row(effect_row);
        append_effect_row(effect_row->pane, TRUE);

        return;
    }

    /* Replace effect */
    if (position < chain->filters->len) {
        CamoramaFilter *old_filter = g_ptr_array_index(chain->filters,
                                                        position);
        CamoramaFilter *filter = g_object_new(selected_type, NULL);

        camorama_filter_show(filter, effect_row->pane->cam);
        camorama_filter_hide(old_filter);
        g_ptr_array_index(chain->filters, position) = filter;
        g_object_unref(old_filter);
    }
    effect_row->selected_type = selected_type;
    update_effect_row(effect_row);
}

#if GTK_MAJOR_VERSION < 4
static void effect_choice_changed(GtkComboBox *, struct effect_row *effect_row)
{
    effect_changed(effect_row);
}
#else
static void effect_choice_changed(GtkDropDown *, GParamSpec *,
                                  struct effect_row *effect_row)
{
    if (!effect_row->refresh_pending)
        effect_changed(effect_row);
}
#endif

static void effect_row_free(struct effect_row *effect_row)
{
    clear_source(&effect_row->animation_source);
#if GTK_MAJOR_VERSION >= 4
    clear_source(&effect_row->refresh_source);
    clear_source(&effect_row->remove_source);
#endif
    g_array_unref(effect_row->choices);
    g_free(effect_row);
}

void camorama_effects_shutdown(cam_t *cam)
{
    GtkListBox *list = GTK_LIST_BOX(gtk_builder_get_object(cam->xml,
                                                           "effects_list"));
#if GTK_MAJOR_VERSION >= 4
    GtkWidget *row;

    while ((row = gtk_widget_get_first_child(GTK_WIDGET(list)))) {
        struct effect_row *effect_row = g_object_get_data(G_OBJECT(row),
                                                          "effect-row");

        if (effect_row)
            prepare_effect_row_destroy(effect_row, TRUE);

        gtk_list_box_remove(list, row);
    }
#endif

    g_object_set_data(G_OBJECT(list), "filter-chain", NULL);
}

static void append_effect_row(struct effects_pane *pane, gboolean animate)
{
    struct effect_row *effect_row = g_new0(struct effect_row, 1);
    GtkWidget *content = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);

    effect_row->pane = pane;
    effect_row->row = gtk_list_box_row_new();
    effect_row->revealer = GTK_REVEALER(gtk_revealer_new());
#if GTK_MAJOR_VERSION < 4
    effect_row->choice = GTK_COMBO_BOX_TEXT(gtk_combo_box_text_new());
#else
    effect_row->model = gtk_string_list_new(NULL);
    effect_row->choice = GTK_DROP_DOWN(gtk_drop_down_new(
        G_LIST_MODEL(effect_row->model), NULL));
#endif
    effect_row->choices = g_array_new(FALSE, FALSE, sizeof(GType));

    gtk_widget_set_margin_top(content, 6);
    gtk_widget_set_margin_bottom(content, 6);
    gtk_widget_set_margin_start(content, 6);
    gtk_widget_set_margin_end(content, 6);
    gtk_widget_set_hexpand(GTK_WIDGET(effect_row->choice), TRUE);

    g_array_set_size(effect_row->choices, 0);
    fill_effect_choices(effect_row, FALSE);

    set_choice_active(effect_row, 0);

#if GTK_MAJOR_VERSION < 4
    effect_row->changed_handler = g_signal_connect(effect_row->choice,
                                                   "changed",
                                                   G_CALLBACK(effect_choice_changed),
                                                   effect_row);
#else
    effect_row->changed_handler = g_signal_connect(effect_row->choice,
                                                   "notify::selected",
                                                   G_CALLBACK(effect_choice_changed),
                                                   effect_row);
#endif

    gtk_revealer_set_transition_type(
        effect_row->revealer, GTK_REVEALER_TRANSITION_TYPE_SLIDE_DOWN);
    gtk_revealer_set_transition_duration(effect_row->revealer,
                                         EFFECT_TRANSITION_DURATION);
    gtk_revealer_set_reveal_child(effect_row->revealer, !animate);

#if GTK_MAJOR_VERSION < 4
    gtk_container_add(GTK_CONTAINER(content),
                      GTK_WIDGET(effect_row->choice));
    gtk_container_add(GTK_CONTAINER(effect_row->revealer), content);
    gtk_container_add(GTK_CONTAINER(effect_row->row),
                      GTK_WIDGET(effect_row->revealer));
    gtk_list_box_insert(pane->list, effect_row->row, -1);
    gtk_widget_show_all(effect_row->row);
#else
    gtk_box_append(GTK_BOX(content), GTK_WIDGET(effect_row->choice));
    gtk_revealer_set_child(effect_row->revealer, content);
    gtk_list_box_row_set_child(GTK_LIST_BOX_ROW(effect_row->row),
                               GTK_WIDGET(effect_row->revealer));
    gtk_list_box_append(pane->list, effect_row->row);
#endif

    g_object_set_data_full(G_OBJECT(effect_row->row), "effect-row",
                           effect_row, (GDestroyNotify)effect_row_free);

#if GTK_MAJOR_VERSION < 4
    g_signal_connect(effect_row->revealer, "notify::child-revealed",
                     G_CALLBACK(remove_effect_row), effect_row);
#else
    effect_row->revealer_handler =
        g_signal_connect(effect_row->revealer, "notify::child-revealed",
                         G_CALLBACK(remove_effect_row), effect_row);
#endif

    if (animate) {
        effect_row->animation_source = g_idle_add_full(
            G_PRIORITY_DEFAULT_IDLE, reveal_effect_row,
            g_object_ref(effect_row->row), g_object_unref);
    }
}

static void effects_pane_free(struct effects_pane *pane)
{
    g_ptr_array_unref(pane->effects);
    g_free(pane);
}

static void effect_info_free(struct effect_info *effect)
{
    g_free(effect->name);
    g_free(effect);
}

void load_interface(cam_t *cam)
{
    unsigned int i, n_filters;
    GtkWidget *video_dev;
    GtkWidget *window = GTK_WIDGET(gtk_builder_get_object(cam->xml,
                                                          "main_window"));
    struct effects_pane *pane = g_new0(struct effects_pane, 1);
    GType *filter_types;

    gtk_application_add_window(cam->app, GTK_WINDOW(window));

    gtk_widget_show(window);

    prefswindow = GTK_WIDGET(gtk_builder_get_object(cam->xml, "prefswindow"));

    pane->cam = cam;
    pane->list = GTK_LIST_BOX(gtk_builder_get_object(cam->xml,
                                                     "effects_list"));
    pane->effects = g_ptr_array_new_with_free_func((GDestroyNotify)effect_info_free);

    filter_types = g_type_children(CAMORAMA_TYPE_FILTER, &n_filters);
    for (i = 0; i < n_filters; i++) {
        CamoramaFilterClass *filter_class = g_type_class_ref(filter_types[i]);
        struct effect_info *effect = g_new0(struct effect_info, 1);

        effect->type = filter_types[i];
        effect->name = g_strdup(filter_class->name ?
                                filter_class->name : g_type_name(filter_types[i]));

        g_ptr_array_add(pane->effects, effect);
        g_type_class_unref(filter_class);
    }
    g_free(filter_types);

    cam->filter_chain = camorama_filter_chain_new();
    camorama_filter_chain_set_data(cam->filter_chain, cam);
    g_object_set_data_full(G_OBJECT(pane->list), "effects-pane", pane,
                           (GDestroyNotify)effects_pane_free);
    g_object_set_data_full(G_OBJECT(pane->list), "filter-chain",
                           g_object_ref(cam->filter_chain), g_object_unref);

    append_effect_row(pane, FALSE);


    if (!cam->show_effects) {
        GtkWidget *effects = GTK_WIDGET(gtk_builder_get_object(cam->xml,
                                                               "scrolledwindow_effects"));
        if (effects)
            gtk_widget_hide(effects);
    }

    /* connect the signals in the interface
     * glade_xml_signal_autoconnect(xml);
     * this won't work, can't pass data to callbacks.  have to do it individually :(*/

    gtk_common_set_window_icons(GTK_WINDOW(window), GTK_WINDOW(prefswindow));

#if GTK_MAJOR_VERSION < 4
    g_signal_connect(prefswindow, "delete-event",
                     G_CALLBACK(gtk3_close_prefs_window), cam);
#else
    g_signal_connect(prefswindow, "close-request",
                     G_CALLBACK(gtk4_close_prefs_window), cam);
#endif

    g_object_set(gtk_builder_get_object(cam->xml, "showadjustment_item"),
                 "active", cam->show_adjustments, NULL);
    g_signal_connect(gtk_builder_get_object(cam->xml, "showadjustment_item"),
                     "toggled", G_CALLBACK(on_show_adjustments_activate), cam);
    g_object_set(gtk_builder_get_object(cam->xml, "show_effects"),
                 "active", cam->show_effects, NULL);
    g_signal_connect(gtk_builder_get_object(cam->xml, "show_effects"),
                     "toggled", G_CALLBACK(on_show_effects_activate), cam);

    gtk_common_set_toggle_active(GTK_WIDGET(gtk_builder_get_object(cam->xml,
                                                                    "togglebutton1")),
                                 cam->show_adjustments);
    g_signal_connect(gtk_builder_get_object(cam->xml, "togglebutton1"),
                     "toggled", G_CALLBACK(on_show_adjustments_activate),
                     cam);

    if (n_valid_devices > 1) {
        video_dev = GTK_WIDGET(gtk_builder_get_object(cam->xml, "change_camera"));
        if (video_dev) {
            gtk_widget_show(video_dev);
            g_signal_connect(video_dev, "clicked",
                             G_CALLBACK(on_change_camera), cam);
        }
    }

    if (gtk_builder_get_object(cam->xml, "imagemenuitem1"))
        g_signal_connect(gtk_builder_get_object(cam->xml, "imagemenuitem1"),
                         "clicked", G_CALLBACK(capture_func), cam);
    g_signal_connect(gtk_builder_get_object(cam->xml, "button1"),
                     "clicked", G_CALLBACK(capture_func), cam);

    if (gtk_builder_get_object(cam->xml, "show_ctrls"))
        g_signal_connect(gtk_builder_get_object(cam->xml, "show_ctrls"),
                         "clicked", G_CALLBACK(show_controls), cam);

    update_sliders(cam);

    if (cam->show_adjustments == FALSE)
        gtk_widget_hide(GTK_WIDGET(gtk_builder_get_object(cam->xml,
                                                          "adjustments_table")));

    /* buttons */
    if (gtk_builder_get_object(cam->xml, "quit"))
        g_signal_connect(gtk_builder_get_object(cam->xml, "quit"), "clicked",
                         G_CALLBACK(on_quit_activate), cam);
    if (gtk_builder_get_object(cam->xml, "imagemenuitem3"))
        g_signal_connect(gtk_builder_get_object(cam->xml, "imagemenuitem3"),
                         "clicked", G_CALLBACK(on_preferences1_activate),
                         cam);
    if (gtk_builder_get_object(cam->xml, "imagemenuitem4"))
        g_signal_connect(gtk_builder_get_object(cam->xml, "imagemenuitem4"),
                         "clicked", G_CALLBACK(on_about_activate), cam);

    /* prefs */
    g_signal_connect(gtk_builder_get_object(cam->xml, "okbutton1"),
                     "clicked", G_CALLBACK(prefs_func), cam);

    /* general */
    g_signal_connect(gtk_builder_get_object(cam->xml, "captured_cb"),
                     "toggled", G_CALLBACK(cap_func), cam);

    gtk_common_set_toggle_active(GTK_WIDGET(gtk_builder_get_object(cam->xml,
                                                                   "captured_cb")),
                                 cam->cap);

    g_signal_connect(gtk_builder_get_object(cam->xml, "rcapture"),
                     "toggled", G_CALLBACK(rcap_func), cam);
    gtk_common_set_toggle_active(GTK_WIDGET(gtk_builder_get_object(cam->xml,
                                                                   "rcapture")),
                                 cam->rcap);

    g_signal_connect(gtk_builder_get_object(cam->xml, "acapture"),
                     "toggled", G_CALLBACK(acap_func), cam);
    gtk_common_set_toggle_active(
        GTK_WIDGET(gtk_builder_get_object(cam->xml, "acapture")), cam->acap);

    g_signal_connect(gtk_builder_get_object(cam->xml, "interval_entry"),
                     "value-changed", G_CALLBACK(interval_change), cam);

    gtk_spin_button_set_value((GtkSpinButton *)
                              gtk_builder_get_object(cam->xml,
                                                     "interval_entry"),
                              (cam->timeout_interval / 60000));

    /* local */
    dentry = GTK_WIDGET(gtk_builder_get_object(cam->xml, "dentry"));
    entry2 = GTK_WIDGET(gtk_builder_get_object(cam->xml, "entry2"));
    gtk_common_set_file_chooser_folder(dentry, cam->pixdir);

    gtk_common_set_entry_text(entry2, cam->capturefile);

    g_signal_connect(gtk_builder_get_object(cam->xml, "appendbutton"),
                     "toggled", G_CALLBACK(append_func), cam);
    gtk_common_set_toggle_active(GTK_WIDGET(gtk_builder_get_object(cam->xml,
                                                                   "appendbutton")),
                                 cam->timefn);

    g_signal_connect(gtk_builder_get_object(cam->xml, "jpgb"),
                     "toggled", G_CALLBACK(jpg_func), cam);
    if (cam->savetype == JPEG) {
        gtk_common_set_toggle_active(GTK_WIDGET(gtk_builder_get_object(cam->xml,
                                                                       "jpgb")),
                                     TRUE);
    }
    g_signal_connect(gtk_builder_get_object(cam->xml, "pngb"),
                     "toggled", G_CALLBACK(png_func), cam);
    if (cam->savetype == PNG) {
        gtk_common_set_toggle_active(GTK_WIDGET(gtk_builder_get_object(cam->xml,
                                                                       "pngb")),
                                     TRUE);
    }

    g_signal_connect(gtk_builder_get_object(cam->xml, "tsbutton"),
                     "toggled", G_CALLBACK(ts_func), cam);
    gtk_common_set_toggle_active(GTK_WIDGET(gtk_builder_get_object(cam->xml,
                                                                   "tsbutton")),
                                 cam->timestamp);

    /* remote */
    host_entry = GTK_WIDGET(gtk_builder_get_object(cam->xml, "host_entry"));
    protocol = GTK_WIDGET(gtk_builder_get_object(cam->xml, "remote_protocol"));
    rdir_entry = GTK_WIDGET(gtk_builder_get_object(cam->xml, "rdir_entry"));
    filename_entry = GTK_WIDGET(gtk_builder_get_object(cam->xml,
                                                       "filename_entry"));

    gtk_common_set_entry_text(host_entry, cam->host);
    gtk_common_set_entry_text(rdir_entry, cam->rdir);
    gtk_common_set_entry_text(filename_entry, cam->rcapturefile);

    if (!cam->proto)
        cam->proto = g_strdup(protos[0]);

    for (i = 0; i < G_N_ELEMENTS(protos); i++) {
        gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(protocol),
                                       protos[i]);
        if (!strcmp(cam->proto, protos[i]))
            gtk_combo_box_set_active(GTK_COMBO_BOX(protocol), i);
    }

    if (cam->cap && cam->host && cam->proto && cam->rdir) {
        cam->uri = volume_uri(cam->host, cam->proto, cam->rdir);
        mount_volume(cam);
    } else {
        cam->uri = NULL;
    }

    g_signal_connect(gtk_builder_get_object(cam->xml, "timecb"),
                     "toggled", G_CALLBACK(rappend_func), cam);
    gtk_common_set_toggle_active(GTK_WIDGET(gtk_builder_get_object(cam->xml,
                                                                   "timecb")),
                                 cam->rtimefn);

    g_signal_connect(gtk_builder_get_object(cam->xml, "fjpgb"),
                     "toggled", G_CALLBACK(rjpg_func), cam);
    if (cam->rsavetype == JPEG) {
        gtk_common_set_toggle_active(GTK_WIDGET(gtk_builder_get_object(cam->xml,
                                                                       "fjpgb")),
                                     TRUE);
    }
    g_signal_connect(gtk_builder_get_object(cam->xml, "fpngb"),
                     "toggled", G_CALLBACK(rpng_func), cam);
    if (cam->rsavetype == PNG) {
        gtk_common_set_toggle_active(GTK_WIDGET(gtk_builder_get_object(cam->xml,
                                                                       "fpngb")),
                                     TRUE);
    }

    g_signal_connect(gtk_builder_get_object(cam->xml, "tsbutton2"),
                     "toggled", G_CALLBACK(rts_func), cam);
    gtk_common_set_toggle_active(GTK_WIDGET(gtk_builder_get_object(cam->xml,
                                                                   "tsbutton2")),
                                 cam->rtimestamp);

    /* timestamp */
    g_signal_connect(gtk_builder_get_object(cam->xml, "cscb"),
                     "toggled", G_CALLBACK(customstring_func), cam);
    gtk_common_set_toggle_active(GTK_WIDGET(gtk_builder_get_object(cam->xml,
                                                                   "cscb")),
                                 cam->usestring);

    string_entry = GTK_WIDGET(gtk_builder_get_object(cam->xml, "string_entry"));
    gtk_common_set_entry_text(string_entry, cam->ts_string);

    g_signal_connect(gtk_builder_get_object(cam->xml, "tscb"),
                     "toggled", G_CALLBACK(drawdate_func), cam);
    gtk_common_set_toggle_active(GTK_WIDGET(gtk_builder_get_object(cam->xml,
                                                                   "tscb")),
                                 cam->usedate);

    GtkWidget *status = GTK_WIDGET(gtk_builder_get_object(cam->xml, "status"));

    cam->status = NULL;
    if (GTK_IS_STATUSBAR(status))
        cam->status = g_object_ref(status);

    if (!cam->status) {
        g_warning("Unable to locate GtkStatusbar status widget in UI");
    }

    set_sensitive(cam);
    gtk_widget_set_sensitive(GTK_WIDGET(gtk_builder_get_object(cam->xml, "string_entry")),
                             cam->usestring);

    // Detect window resize calls
#if GTK_MAJOR_VERSION < 4
    g_signal_connect(GTK_WIDGET(gtk_builder_get_object(cam->xml, "da")),
                     "configure-event", G_CALLBACK(on_configure_event), cam);
    g_signal_connect(window, "window-state-event",
                     G_CALLBACK(gtk3_on_window_state_event), cam);
#else
    g_signal_connect(window, "notify::fullscreened",
                     G_CALLBACK(gtk4_fullscreen_changed), cam);
#endif

    g_signal_connect(gtk_builder_get_object(cam->xml, "button3"),
                     "clicked", G_CALLBACK(toggle_fullscreen), cam);
    if (gtk_builder_get_object(cam->xml, "imagemenuitem2"))
        g_signal_connect(gtk_builder_get_object(cam->xml, "imagemenuitem2"),
                         "clicked", G_CALLBACK(toggle_fullscreen), cam);
}
