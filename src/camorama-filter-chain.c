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

CamoramaFilterChain *camorama_filter_chain_new(void)
{
    return g_object_new(CAMORAMA_TYPE_FILTER_CHAIN, NULL);
}

static cam_t *camorama_filter_chain_lock(CamoramaFilterChain *self)
{
    cam_t *cam = self->data;

    if (cam)
        g_mutex_lock(&cam->display_mutex);
    return cam;
}

static void camorama_filter_chain_unlock(cam_t *cam)
{
    if (cam)
        g_mutex_unlock(&cam->display_mutex);
}

void camorama_filter_chain_apply(CamoramaFilterChain *self,
                                 guchar *image, gint width, gint height,
                                 gint depth)
{
    guint i;
    cam_t *cam = camorama_filter_chain_lock(self);

    for (i = 0; i < self->filters->len; i++) {
        CamoramaFilter *filter = g_ptr_array_index(self->filters, i);

        camorama_filter_apply(filter, image, width, height, depth);
    }
    camorama_filter_chain_unlock(cam);
}

void camorama_filter_chain_insert(CamoramaFilterChain *self, guint position,
                                  CamoramaFilter *filter)
{
    cam_t *cam = camorama_filter_chain_lock(self);

    g_ptr_array_insert(self->filters, position, filter);
    camorama_filter_chain_unlock(cam);
}

void camorama_filter_chain_remove(CamoramaFilterChain *self, guint position)
{
    cam_t *cam = camorama_filter_chain_lock(self);

    if (position < self->filters->len) {
        camorama_filter_hide(g_ptr_array_index(self->filters, position));
        g_ptr_array_remove_index(self->filters, position);
    }
    camorama_filter_chain_unlock(cam);
}

void camorama_filter_chain_replace(CamoramaFilterChain *self, guint position,
                                   CamoramaFilter *filter)
{
    cam_t *cam = camorama_filter_chain_lock(self);

    if (position < self->filters->len) {
        CamoramaFilter *old_filter = g_ptr_array_index(self->filters, position);

        camorama_filter_hide(old_filter);
        g_ptr_array_index(self->filters, position) = filter;
        g_object_unref(old_filter);
    } else {
        g_object_unref(filter);
    }
    camorama_filter_chain_unlock(cam);
}

void camorama_filter_chain_set_data(CamoramaFilterChain *self,
                                    gpointer user_data)
{
    self->data = user_data;
}

/* GType stuff */
G_DEFINE_TYPE(CamoramaFilterChain, camorama_filter_chain,
              G_TYPE_OBJECT);

static void camorama_filter_chain_init(CamoramaFilterChain *self)
{
    self->filters = g_ptr_array_new_with_free_func(g_object_unref);
}

static void camorama_filter_chain_finalize(GObject *object)
{
    CamoramaFilterChain *self = (CamoramaFilterChain *)object;
    guint i;

    for (i = 0; i < self->filters->len; i++)
        camorama_filter_hide(g_ptr_array_index(self->filters, i));
    g_ptr_array_unref(self->filters);
    G_OBJECT_CLASS(camorama_filter_chain_parent_class)->finalize(object);
}

static void camorama_filter_chain_class_init(
    CamoramaFilterChainClass *self_class)
{
    G_OBJECT_CLASS(self_class)->finalize = camorama_filter_chain_finalize;
}
