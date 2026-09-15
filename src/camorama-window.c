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
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
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
#include "support.h"
#if GTK_MAJOR_VERSION < 4
#include "gtk3-effects.h"
#endif

/* Supported URI protocol schemas */
const gchar *const protos[3] = { "ftp", "sftp", "smb" };

void load_interface(cam_t *cam)
{
    unsigned int i;
#if GTK_MAJOR_VERSION >= 4
    GtkCellRenderer *cell;
#endif
    GtkWidget *video_dev;
    GtkWidget *window = GTK_WIDGET(gtk_builder_get_object(cam->xml,
                                                          "main_window"));
#if GTK_MAJOR_VERSION >= 4
    GtkTreeView *treeview;
#endif

    gtk_application_add_window(cam->app, GTK_WINDOW(window));

    gtk_widget_set_visible(window, TRUE);;

    prefswindow = GTK_WIDGET(gtk_builder_get_object(cam->xml, "prefswindow"));

#if GTK_MAJOR_VERSION < 4
    gtk3_effects_setup(cam);
#else
    /* set up the tree view */
    treeview = GTK_TREE_VIEW(gtk_builder_get_object(cam->xml,
                                                    "treeview_effects"));
    cell = gtk_cell_renderer_text_new();
    g_object_set(cell, "ellipsize", PANGO_ELLIPSIZE_END, NULL);
    gtk_cell_renderer_text_set_fixed_height_from_font
        (GTK_CELL_RENDERER_TEXT(cell), 1);
    gtk_tree_view_insert_column_with_attributes(treeview, -1, _("Effects"),
                                                cell, "text",
                                                CAMORAMA_FILTER_CHAIN_COL_NAME,
                                                NULL);
    cam->filter_chain = camorama_filter_chain_new();
    camorama_filter_chain_set_data(cam->filter_chain, cam);

    gtk_tree_view_set_model(treeview, GTK_TREE_MODEL(cam->filter_chain));
    gtk_common_setup_effects_popup(treeview);
#endif

    if (!cam->show_effects) {
        GtkWidget *effects = GTK_WIDGET(gtk_builder_get_object
                                        (cam->xml, "scrolledwindow_effects"));
        if (effects)
            gtk_widget_set_visible(effects, FALSE);;
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

    gtk_common_set_toggle_active(
        GTK_WIDGET(gtk_builder_get_object(cam->xml, "togglebutton1")),
        cam->show_adjustments);
    g_signal_connect(gtk_builder_get_object(cam->xml, "togglebutton1"),
                     "toggled", G_CALLBACK(on_show_adjustments_activate),
                     cam);

    if (n_valid_devices > 1) {
        video_dev = GTK_WIDGET(gtk_builder_get_object(cam->xml, "change_camera"));
        if (video_dev) {
            gtk_widget_set_visible(video_dev, TRUE);;
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
        gtk_widget_set_visible(GTK_WIDGET(gtk_builder_get_object(cam->xml,
                                                          "adjustments_table")), FALSE);;

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

    gtk_common_set_toggle_active(
        GTK_WIDGET(gtk_builder_get_object(cam->xml, "captured_cb")),
        cam->cap);

    g_signal_connect(gtk_builder_get_object(cam->xml, "rcapture"),
                     "toggled", G_CALLBACK(rcap_func), cam);
    gtk_common_set_toggle_active(
        GTK_WIDGET(gtk_builder_get_object(cam->xml, "rcapture")), cam->rcap);

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
    gtk_common_set_toggle_active(
        GTK_WIDGET(gtk_builder_get_object(cam->xml, "appendbutton")),
        cam->timefn);

    g_signal_connect(gtk_builder_get_object(cam->xml, "jpgb"),
                     "toggled", G_CALLBACK(jpg_func), cam);
    if (cam->savetype == JPEG) {
        gtk_common_set_toggle_active(
            GTK_WIDGET(gtk_builder_get_object(cam->xml, "jpgb")), TRUE);
    }
    g_signal_connect(gtk_builder_get_object(cam->xml, "pngb"),
                     "toggled", G_CALLBACK(png_func), cam);
    if (cam->savetype == PNG) {
        gtk_common_set_toggle_active(
            GTK_WIDGET(gtk_builder_get_object(cam->xml, "pngb")), TRUE);
    }

    g_signal_connect(gtk_builder_get_object(cam->xml, "tsbutton"),
                     "toggled", G_CALLBACK(ts_func), cam);
    gtk_common_set_toggle_active(
        GTK_WIDGET(gtk_builder_get_object(cam->xml, "tsbutton")),
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
    gtk_common_set_toggle_active(
        GTK_WIDGET(gtk_builder_get_object(cam->xml, "timecb")), cam->rtimefn);

    g_signal_connect(gtk_builder_get_object(cam->xml, "fjpgb"),
                     "toggled", G_CALLBACK(rjpg_func), cam);
    if (cam->rsavetype == JPEG) {
        gtk_common_set_toggle_active(
            GTK_WIDGET(gtk_builder_get_object(cam->xml, "fjpgb")),
            TRUE);
    }
    g_signal_connect(gtk_builder_get_object(cam->xml, "fpngb"),
                     "toggled", G_CALLBACK(rpng_func), cam);
    if (cam->rsavetype == PNG) {
        gtk_common_set_toggle_active(
            GTK_WIDGET(gtk_builder_get_object(cam->xml, "fpngb")),
            TRUE);
    }

    g_signal_connect(gtk_builder_get_object(cam->xml, "tsbutton2"),
                     "toggled", G_CALLBACK(rts_func), cam);
    gtk_common_set_toggle_active(
        GTK_WIDGET(gtk_builder_get_object(cam->xml, "tsbutton2")),
        cam->rtimestamp);

    /* timestamp */
    g_signal_connect(gtk_builder_get_object(cam->xml, "cscb"),
                     "toggled", G_CALLBACK(customstring_func), cam);
    gtk_common_set_toggle_active(
        GTK_WIDGET(gtk_builder_get_object(cam->xml, "cscb")), cam->usestring);

    string_entry = GTK_WIDGET(gtk_builder_get_object(cam->xml, "string_entry"));
    gtk_common_set_entry_text(string_entry, cam->ts_string);

    g_signal_connect(gtk_builder_get_object(cam->xml, "tscb"),
                     "toggled", G_CALLBACK(drawdate_func), cam);
    gtk_common_set_toggle_active(
        GTK_WIDGET(gtk_builder_get_object(cam->xml, "tscb")), cam->usedate);

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
                     G_CALLBACK(on_window_state_event), cam);
#else
    g_signal_connect(window, "notify::fullscreened",
                     G_CALLBACK(on_window_fullscreen_changed), cam);
#endif

    g_signal_connect(gtk_builder_get_object(cam->xml, "button3"),
                     "clicked", G_CALLBACK(toggle_fullscreen), cam);
    if (gtk_builder_get_object(cam->xml, "imagemenuitem2"))
        g_signal_connect(gtk_builder_get_object(cam->xml, "imagemenuitem2"),
                         "clicked", G_CALLBACK(toggle_fullscreen), cam);
}
