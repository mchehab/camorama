/*
 * Copyright © 2011 by Mauro Carvalho Chehab <mchehab@kernel.org>
 *
 * This file is derived from tvtime's get_media_devices helper, which is
 * licensed under the GNU Lesser General Public License, version 2.1 or later.
 */

#include "media-devices.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum media_device_type {
    MEDIA_DEVICE_VIDEO,
    MEDIA_DEVICE_SOUND_CAPTURE,
};

struct media_device {
    gchar *device;
    gchar *node;
    enum media_device_type type;
};

static gchar *get_device_path(const gchar *class, const gchar *node)
{
    gchar *path;
    gchar *device;
    gchar *class_path;
    gchar *last_component;
    gchar *modalias;
    gchar *contents = NULL;

    path = g_strdup_printf("/sys/class/%s/%s", class, node);
    device = realpath(path, NULL);
    g_free(path);
    if (!device)
        return NULL;

    class_path = strstr(device, class);
    if (!class_path || class_path == device) {
        free(device);
        return NULL;
    }
    *(class_path - 1) = '\0';

    modalias = g_build_filename(device, "modalias", NULL);
    if (!g_file_get_contents(modalias, &contents, NULL, NULL)) {
        g_free(modalias);
        free(device);
        return NULL;
    }
    g_free(modalias);

    if (g_str_has_prefix(contents, "usb")) {
        last_component = strrchr(device, '/');
        if (last_component && strchr(last_component, ':'))
            *last_component = '\0';
    } else if (g_str_has_prefix(contents, "pci")) {
        last_component = strrchr(device, '.');
        if (last_component)
            *last_component = '\0';
    }
    g_free(contents);

    path = g_strdup(device + strlen("/sys/devices/"));
    free(device);
    return path;
}

static void add_media_device(GPtrArray *devices, const gchar *class,
                             const gchar *node, enum media_device_type type)
{
    struct media_device *device;

    device = g_new0(struct media_device, 1);
    device->device = get_device_path(class, node);
    if (!device->device) {
        g_free(device);
        return;
    }
    device->node = g_strdup(node);
    device->type = type;
    g_ptr_array_add(devices, device);
}

static void add_class(GPtrArray *devices, const gchar *class)
{
    GDir *dir;
    const gchar *node;
    gchar *path;

    path = g_strdup_printf("/sys/class/%s", class);
    dir = g_dir_open(path, 0, NULL);
    g_free(path);
    if (!dir)
        return;

    while ((node = g_dir_read_name(dir))) {
        if (!strcmp(class, "video4linux") && g_str_has_prefix(node, "video"))
            add_media_device(devices, class, node, MEDIA_DEVICE_VIDEO);
        else if (!strcmp(class, "sound") && g_str_has_prefix(node, "pcm") &&
                 g_str_has_suffix(node, "c"))
            add_media_device(devices, class, node, MEDIA_DEVICE_SOUND_CAPTURE);
    }
    g_dir_close(dir);
}

static gint compare_media_devices(gconstpointer a, gconstpointer b)
{
    const struct media_device *device_a = *(struct media_device * const *)a;
    const struct media_device *device_b = *(struct media_device * const *)b;
    gint ret;

    ret = strcmp(device_a->device, device_b->device);
    if (ret)
        return ret;
    ret = device_a->type - device_b->type;
    if (ret)
        return ret;
    return strcmp(device_a->node, device_b->node);
}

static void free_media_device(gpointer data)
{
    struct media_device *device = data;

    g_free(device->device);
    g_free(device->node);
    g_free(device);
}

gint cam_media_audio_card(const gchar *video_dev, gboolean debug)
{
    GPtrArray *devices;
    gchar *video_path;
    gchar *video_node;
    gboolean video_is_path;
    guint i;
    gint card = -1;

    devices = g_ptr_array_new_with_free_func(free_media_device);
    add_class(devices, "video4linux");
    add_class(devices, "sound");
    g_ptr_array_sort(devices, compare_media_devices);

    video_path = realpath(video_dev, NULL);
    video_is_path = video_path != NULL || g_path_is_absolute(video_dev);
    video_node = g_path_get_basename(video_path ? video_path : video_dev);
    free(video_path);
    for (i = 0; i < devices->len; i++) {
        struct media_device *video = g_ptr_array_index(devices, i);
        guint j;

        if (video->type != MEDIA_DEVICE_VIDEO)
            continue;
        if (video_is_path && strcmp(video->node, video_node))
            continue;
        if (!video_is_path && strcmp(video->device, video_dev))
            continue;

        for (j = i + 1; j < devices->len; j++) {
            struct media_device *audio = g_ptr_array_index(devices, j);

            if (strcmp(video->device, audio->device))
                break;
            if (audio->type != MEDIA_DEVICE_SOUND_CAPTURE)
                continue;
            if (sscanf(audio->node, "pcmC%dD%*dc", &card) == 1 && debug)
                printf("Media devices: V4L %s matches capture hw:%d (%s)\n",
                       video_node, card, video->device);
            break;
        }
        break;
    }
    if (debug && card < 0)
        printf("Media devices: no capture device matches V4L %s\n", video_node);
    g_free(video_node);
    g_ptr_array_unref(devices);

    return card;
}
