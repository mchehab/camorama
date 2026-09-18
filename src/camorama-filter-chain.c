/* This file is part of camorama
 *
 * AUTHORS
 *     Sven Herzberg  <herzi@gnome-de.org>
 *
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

#include "v4l.h"
#include "camorama-filter-chain.h"

#include "filter.h"

struct CamoramaImage {
    guchar *buf;
    gint width;
    gint height;
    gint depth;
};

#if GTK_MAJOR_VERSION < 4
CamoramaFilterChain *camorama_filter_chain_new(void)
{
    return g_object_new(CAMORAMA_TYPE_FILTER_CHAIN, NULL);
}

static void camorama_filter_chain_set_filter(CamoramaFilterChain *self,
                                             GtkTreeIter *iter,
                                             GType filter_type)
{
    CamoramaFilter *filter;
    gpointer data;

    g_return_if_fail(g_type_is_a(filter_type, CAMORAMA_TYPE_FILTER));

    filter = g_object_new(filter_type, NULL);
    data = CAMORAMA_FILTER_CHAIN_GET_CLASS(self)->data;
    camorama_filter_show(filter, data);

    gtk_list_store_set(GTK_LIST_STORE(self), iter,
                       CAMORAMA_FILTER_CHAIN_COL_FILTER, filter,
                       CAMORAMA_FILTER_CHAIN_COL_NAME,
                       camorama_filter_get_name(filter), -1);
    g_object_unref(filter);
}

void camorama_filter_chain_append(CamoramaFilterChain *self,
                                  GType filter_type)
{
    GtkTreeIter iter;

    gtk_list_store_append(GTK_LIST_STORE(self), &iter);
    camorama_filter_chain_set_filter(self, &iter, filter_type);
}

void camorama_filter_chain_insert(CamoramaFilterChain *self,
                                  guint position, GType filter_type)
{
    GtkTreeIter iter;

    gtk_list_store_insert(GTK_LIST_STORE(self), &iter, position);
    camorama_filter_chain_set_filter(self, &iter, filter_type);
}

void camorama_filter_chain_replace(CamoramaFilterChain *self,
                                   guint position, GType filter_type)
{
    GtkTreeIter iter;

    if (!gtk_tree_model_iter_nth_child(GTK_TREE_MODEL(self), &iter, NULL,
                                       position))
        return;

    camorama_filter_chain_hide(GTK_TREE_MODEL(self), NULL, &iter);
    camorama_filter_chain_set_filter(self, &iter, filter_type);
}

void camorama_filter_chain_remove(CamoramaFilterChain *self,
                                  guint position)
{
    GtkTreeIter iter;

    if (!gtk_tree_model_iter_nth_child(GTK_TREE_MODEL(self), &iter, NULL,
                                       position))
        return;

    camorama_filter_chain_hide(GTK_TREE_MODEL(self), NULL, &iter);
    gtk_list_store_remove(GTK_LIST_STORE(self), &iter);
}

static gboolean camorama_filter_chain_apply_filter(GtkTreeModel *model,
                                                   GtkTreePath *,
                                                   GtkTreeIter *iter,
                                                   gpointer user_data)
{
    struct CamoramaImage *img = user_data;
    CamoramaFilter *filter = NULL;

    gtk_tree_model_get(model, iter,
                       CAMORAMA_FILTER_CHAIN_COL_FILTER, &filter, -1);
    camorama_filter_apply(filter, img->buf, img->width, img->height,
                          img->depth);
    g_object_unref(filter);

    return FALSE;
}

void camorama_filter_chain_hide(GtkTreeModel *model,
                                    GtkTreePath *,
                                    GtkTreeIter *iter)
{
    CamoramaFilter *filter = NULL;

    gtk_tree_model_get(model, iter,
                       CAMORAMA_FILTER_CHAIN_COL_FILTER, &filter, -1);
    camorama_filter_hide(filter);
    g_object_unref(filter);
}

void camorama_filter_chain_apply(CamoramaFilterChain *self,
                                 guchar *image, gint width, gint height,
                                 gint depth)
{
    struct CamoramaImage img = { image, width, height, depth };

    gtk_tree_model_foreach(GTK_TREE_MODEL(self),
                           camorama_filter_chain_apply_filter, &img);
}

void camorama_filter_chain_set_data(CamoramaFilterChain *self,
                                    gpointer user_data)
{
    CAMORAMA_FILTER_CHAIN_GET_CLASS(self)->data = user_data;
}

/* GType stuff */
G_DEFINE_TYPE(CamoramaFilterChain, camorama_filter_chain,
              GTK_TYPE_LIST_STORE);

static void camorama_filter_chain_init(CamoramaFilterChain *self)
{
    GType column_types[CAMORAMA_FILTER_CHAIN_N_COLUMNS];

    column_types[CAMORAMA_FILTER_CHAIN_COL_NAME] = G_TYPE_STRING;
    column_types[CAMORAMA_FILTER_CHAIN_COL_FILTER] = G_TYPE_OBJECT;
    gtk_list_store_set_column_types(GTK_LIST_STORE(self),
                                    CAMORAMA_FILTER_CHAIN_N_COLUMNS,
                                    column_types);
}

static gboolean camorama_filter_chain_hide_filter(GtkTreeModel *model,
                                                  GtkTreePath *path,
                                                  GtkTreeIter *iter,
                                                  gpointer)
{
    camorama_filter_chain_hide(model, path, iter);
    return FALSE;
}

static void camorama_filter_chain_dispose(GObject *object)
{
    CamoramaFilterChain *self = (CamoramaFilterChain *)object;

    gtk_tree_model_foreach(GTK_TREE_MODEL(self),
                           camorama_filter_chain_hide_filter, NULL);
    gtk_list_store_clear(GTK_LIST_STORE(self));
    G_OBJECT_CLASS(camorama_filter_chain_parent_class)->dispose(object);
}

static void camorama_filter_chain_class_init(
    CamoramaFilterChainClass *self_class)
{
    G_OBJECT_CLASS(self_class)->dispose = camorama_filter_chain_dispose;
}
#else
CamoramaFilterChain *camorama_filter_chain_new(void)
{
    return g_object_new(CAMORAMA_TYPE_FILTER_CHAIN, NULL);
}

static CamoramaFilter *camorama_filter_chain_create_filter(
    CamoramaFilterChain *self, GType filter_type)
{
    CamoramaFilter *filter;

    g_return_val_if_fail(g_type_is_a(filter_type, CAMORAMA_TYPE_FILTER),
                         NULL);

    filter = g_object_new(filter_type, NULL);
    camorama_filter_show(filter, self->data);
    return filter;
}

void camorama_filter_chain_append(CamoramaFilterChain *self,
                                  GType filter_type)
{
    CamoramaFilter *filter =
        camorama_filter_chain_create_filter(self, filter_type);

    if (filter)
        g_ptr_array_add(self->filters, filter);
}

void camorama_filter_chain_insert(CamoramaFilterChain *self,
                                  guint position, GType filter_type)
{
    CamoramaFilter *filter =
        camorama_filter_chain_create_filter(self, filter_type);

    if (!filter)
        return;

    position = MIN(position, self->filters->len);
    g_ptr_array_insert(self->filters, position, filter);
}

void camorama_filter_chain_replace(CamoramaFilterChain *self,
                                   guint position, GType filter_type)
{
    CamoramaFilter *old_filter;
    CamoramaFilter *filter;

    if (position >= self->filters->len)
        return;

    filter = camorama_filter_chain_create_filter(self, filter_type);
    if (!filter)
        return;

    old_filter = g_ptr_array_index(self->filters, position);
    camorama_filter_hide(old_filter);
    g_ptr_array_index(self->filters, position) = filter;
    g_object_unref(old_filter);
}

void camorama_filter_chain_remove(CamoramaFilterChain *self,
                                  guint position)
{
    CamoramaFilter *filter;

    if (position >= self->filters->len)
        return;

    filter = g_ptr_array_index(self->filters, position);
    camorama_filter_hide(filter);
    g_ptr_array_remove_index(self->filters, position);
}

void camorama_filter_chain_apply(CamoramaFilterChain *self,
                                 guchar *image, gint width, gint height,
                                 gint depth)
{
    guint i;

    for (i = 0; i < self->filters->len; i++) {
        CamoramaFilter *filter = g_ptr_array_index(self->filters, i);

        camorama_filter_apply(filter, image, width, height, depth);
    }
}

void camorama_filter_chain_set_data(CamoramaFilterChain *self,
                                    gpointer user_data)
{
    self->data = user_data;
}

G_DEFINE_TYPE(CamoramaFilterChain, camorama_filter_chain, G_TYPE_OBJECT);

static void camorama_filter_chain_finalize(GObject *object)
{
    CamoramaFilterChain *self = (CamoramaFilterChain *)object;
    guint i;

    for (i = 0; i < self->filters->len; i++)
        camorama_filter_hide(g_ptr_array_index(self->filters, i));
    g_ptr_array_unref(self->filters);
    G_OBJECT_CLASS(camorama_filter_chain_parent_class)->finalize(object);
}

static void camorama_filter_chain_init(CamoramaFilterChain *self)
{
    self->filters = g_ptr_array_new_with_free_func(g_object_unref);
}

static void camorama_filter_chain_class_init(
    CamoramaFilterChainClass *self_class)
{
    G_OBJECT_CLASS(self_class)->finalize = camorama_filter_chain_finalize;
}
#endif
