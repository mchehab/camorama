#include <gio/gio.h>

#include "camera-backend.h"

#ifndef CAM_SETTINGS_H
#define CAM_SETTINGS_H

static inline gchar *cam_settings_get_string(cam_t *cam,
                                             GSettingsSchema *schema,
                                             const gchar *key)
{
    if (!g_settings_schema_has_key(schema, key))
        return "";

    return g_settings_get_string(cam->gc, key);
}

static inline gboolean cam_settings_get_boolean(cam_t *cam,
                                                GSettingsSchema *schema,
                                                const gchar *key)
{
    if (!g_settings_schema_has_key(schema, key))
        return false;

    return g_settings_get_boolean(cam->gc, key);
}

static inline gint cam_settings_get_int(cam_t *cam,
                                        GSettingsSchema *schema,
                                        const gchar *key)
{
    if (!g_settings_schema_has_key(schema, key))
        return 0;

    return g_settings_get_int(cam->gc, key);
}

static inline gdouble cam_settings_get_double(cam_t *cam,
                                              GSettingsSchema *schema,
                                              const gchar *key)
{
    if (!g_settings_schema_has_key(schema, key))
        return 1.;

    return g_settings_get_double(cam->gc, key);
}

#endif
