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

#ifndef CAMORAMA_FILTER_CHAIN_H
#define CAMORAMA_FILTER_CHAIN_H

G_BEGIN_DECLS

typedef struct _CamoramaFilterChain CamoramaFilterChain;
typedef struct _CamoramaFilterChainClass CamoramaFilterChainClass;

#define CAMORAMA_TYPE_FILTER_CHAIN         (camorama_filter_chain_get_type())
GType camorama_filter_chain_get_type(void);

CamoramaFilterChain *camorama_filter_chain_new(void);
void camorama_filter_chain_apply(CamoramaFilterChain *self,
                                 guchar *image,
                                 gint width, gint height, gint depth);
void camorama_filter_chain_set_data(CamoramaFilterChain *self,
                                    gpointer user_data);

struct _CamoramaFilterChain {
    GObject base_instance;
    GPtrArray *filters;
    gpointer data;
};

struct _CamoramaFilterChainClass {
    GObjectClass base_class;
};

G_END_DECLS
#endif                          /* !CAMORAMA_FILTER_CHAIN_H */
