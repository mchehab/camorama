/*
 * Copyright © 2011 by Mauro Carvalho Chehab <mchehab@kernel.org>
 *
 * This file is derived from tvtime's get_media_devices helper, which is
 * licensed under the GNU Lesser General Public License, version 2.1 or later.
 */

#ifndef CAMORAMA_MEDIA_DEVICES_H
#define CAMORAMA_MEDIA_DEVICES_H

#include <glib.h>

gint cam_media_audio_card(const gchar *video_dev, gboolean debug);

#endif
