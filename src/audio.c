#include "audio.h"
#include "interface.h"
#include "media-devices.h"

#include <limits.h>
#include <pulse/context.h>
#include <pulse/error.h>
#include <pulse/introspect.h>
#include <pulse/mainloop.h>
#include <pulse/proplist.h>
#include <pulse/stream.h>
#include <string.h>
#include <stdlib.h>

/* Scratch buffer: only a copy space for pa_stream_peek(). The actual
 * latency is set by the per-stream buffer attributes below. */
#define AUDIO_BUFFER_FRAMES 4096

struct cam_audio {
    pa_mainloop *mainloop;
    pa_context *context;
    pa_stream *capture;
    pa_stream *playback;
    GThread *thread;
    GMutex lock;
    gboolean stop;
    gboolean capture_ready;
    gboolean playback_ready;
    gboolean debug;
    gdouble volume;
    guint64 bytes;
    gint peak;
    gint64 last_report;
    int16_t *buffer;
    guint channels;
};

static gint get_audio_card(const gchar *video_dev, gboolean debug)
{
    return cam_media_audio_card(video_dev, debug);
}

struct source_lookup {
    gint card;
    gchar *source;
    guint sources;
    gboolean debug;
    gboolean done;
};

struct sink_lookup {
    gchar *sink;
    gboolean done;
};

static void server_info(pa_context *context, const pa_server_info *info,
                        struct sink_lookup *lookup)
{
    (void)context;
    lookup->sink = g_strdup(info->default_sink_name);
    lookup->done = TRUE;
}

static gchar *get_default_sink(gboolean debug)
{
    struct sink_lookup lookup = { 0 };
    pa_mainloop *mainloop;
    pa_context *context;
    pa_operation *operation;
    pa_context_state_t state;
    int ret;

    mainloop = pa_mainloop_new();
    if (!mainloop)
        return NULL;
    context = pa_context_new(pa_mainloop_get_api(mainloop), "Camorama");
    if (!context)
        goto free_mainloop;
    if (pa_context_connect(context, NULL, PA_CONTEXT_NOFLAGS, NULL) < 0)
        goto free_context;

    do {
        if (pa_mainloop_iterate(mainloop, TRUE, &ret) < 0)
            goto disconnect;
        state = pa_context_get_state(context);
    } while (state != PA_CONTEXT_READY &&
             state != PA_CONTEXT_FAILED && state != PA_CONTEXT_TERMINATED);
    if (state != PA_CONTEXT_READY)
        goto disconnect;

    operation = pa_context_get_server_info(context,
                                           (pa_server_info_cb_t)server_info,
                                           &lookup);
    if (!operation)
        goto disconnect;
    while (!lookup.done && pa_operation_get_state(operation) == PA_OPERATION_RUNNING)
        if (pa_mainloop_iterate(mainloop, TRUE, &ret) < 0)
            break;
    pa_operation_unref(operation);
    if (debug)
        printf("PulseAudio: default playback sink: %s\n",
               lookup.sink ? lookup.sink : "(none)");
disconnect:
    pa_context_disconnect(context);
free_context:
    pa_context_unref(context);
free_mainloop:
    pa_mainloop_free(mainloop);
    return lookup.sink;
}

static void source_info(pa_context *context, const pa_source_info *info,
                        int eol, struct source_lookup *lookup)
{
    (void)context;
    gchar const *card;
    gchar const *device;
    char expected[16];

    if (eol) {
        lookup->done = TRUE;
        return;
    }

    card = pa_proplist_gets(info->proplist, "alsa.card");
    if (!card)
        card = pa_proplist_gets(info->proplist, "api.alsa.card");
    device = pa_proplist_gets(info->proplist, PA_PROP_DEVICE_STRING);
    if (!device)
        device = pa_proplist_gets(info->proplist, "api.alsa.path");
    if (lookup->debug)
        printf("PulseAudio source: %s (alsa.card: %s, device: %s)\n",
               info->name, card, device);
    g_snprintf(expected, sizeof(expected), "hw:%d,", lookup->card);
    if (info->monitor_of_sink == PA_INVALID_INDEX) {
        lookup->sources++;
        if ((card && atoi(card) == lookup->card) ||
            (device && g_str_has_prefix(device, expected))) {
            g_free(lookup->source);
            lookup->source = g_strdup(info->name);
        }
    }
}

static gchar *get_audio_source(gint card, gboolean debug)
{
    struct source_lookup lookup = { 0 };
    pa_mainloop *mainloop;
    pa_context *context;
    pa_operation *operation;
    pa_context_state_t state;
    int ret;

    mainloop = pa_mainloop_new();
    if (!mainloop)
        return NULL;
    context = pa_context_new(pa_mainloop_get_api(mainloop), "Camorama");
    if (!context)
        goto free_mainloop;
    if (pa_context_connect(context, NULL, PA_CONTEXT_NOFLAGS, NULL) < 0)
        goto free_context;

    do {
        if (pa_mainloop_iterate(mainloop, TRUE, &ret) < 0)
            goto disconnect;
        state = pa_context_get_state(context);
    } while (state != PA_CONTEXT_READY &&
             state != PA_CONTEXT_FAILED && state != PA_CONTEXT_TERMINATED);
    if (state != PA_CONTEXT_READY)
        goto disconnect;

    lookup.card = card;
    lookup.debug = debug;
    if (debug)
        printf("PulseAudio: looking for card %d\n", card);
    operation = pa_context_get_source_info_list(context,
                                                (pa_source_info_cb_t)source_info,
                                                &lookup);
    if (!operation)
        goto disconnect;
    while (!lookup.done && pa_operation_get_state(operation) == PA_OPERATION_RUNNING)
        if (pa_mainloop_iterate(mainloop, TRUE, &ret) < 0)
            break;
    pa_operation_unref(operation);
    if (debug)
        printf("PulseAudio: found %u recording sources, selected: %s\n",
               lookup.sources, lookup.source);
disconnect:
    pa_context_disconnect(context);
free_context:
    pa_context_unref(context);
free_mainloop:
    pa_mainloop_free(mainloop);
    return lookup.source;
}

gboolean cam_audio_available(cam_t *cam)
{
    gchar *source;
    gint card;

    card = get_audio_card(cam->video_dev, cam->debug);
    if (card < 0) {
        if (cam->debug)
            printf("PulseAudio: no audio card is associated with %s\n",
                   cam->video_dev);
        return FALSE;
    }
    source = get_audio_source(card, cam->debug);
    if (!source)
        return FALSE;
    g_free(source);
    return TRUE;
}

static void stream_state_cb(pa_stream *s, void *userdata)
{
    struct cam_audio *audio = userdata;
    pa_stream_state_t state = pa_stream_get_state(s);

    switch (state) {
    case PA_STREAM_READY:
        if (s == audio->capture)
            audio->capture_ready = TRUE;
        else
            audio->playback_ready = TRUE;
        break;
    case PA_STREAM_FAILED:
        g_mutex_lock(&audio->lock);
        audio->stop = TRUE;
        g_mutex_unlock(&audio->lock);
        break;
    default:
        break;
    }
}

static void stream_suspended_cb(pa_stream *s, void *userdata)
{
    struct cam_audio *audio = userdata;

    /* libpulse reports device suspension not as a state but via this
     * callback; keep the ready flags in sync. */
    if (s == audio->capture)
        audio->capture_ready = !pa_stream_is_suspended(s);
    else
        audio->playback_ready = !pa_stream_is_suspended(s);
}

static void capture_read_cb(pa_stream *s, size_t nbytes, void *userdata)
{
    struct cam_audio *audio = userdata;
    const void *data;
    size_t frag, total = 0;
    uint32_t framesize = audio->channels * (uint32_t)sizeof(int16_t);
    size_t limit = (size_t)AUDIO_BUFFER_FRAMES * framesize;
    gdouble volume;
    int error;

    g_mutex_lock(&audio->lock);
    if (audio->stop) {
        g_mutex_unlock(&audio->lock);
        return;
    }
    volume = audio->volume;
    g_mutex_unlock(&audio->lock);

    /* Consume every fragment the server asked for (pa_stream_peek
     * only returns a single fragment, so loop until the request is
     * fulfilled). Holes (data == NULL with frag > 0) must be dropped
     * too, to advance the read index. */
    while (nbytes > 0) {
        if (pa_stream_peek(s, &data, &frag) < 0 || frag == 0)
            break;
        if (frag > nbytes) {
            /* Fragments cannot be partially consumed: discard the
             * whole fragment and stop processing this request. */
            pa_stream_drop(s);
            break;
        }
        if (!data) {
            /* Hole. */
            pa_stream_drop(s);
        } else if (total + frag <= limit) {
            g_mutex_lock(&audio->lock);
            memcpy(audio->buffer + total, data, frag);
            g_mutex_unlock(&audio->lock);
            pa_stream_drop(s);
            total += frag;
        } else {
            /* No scratch space left: discard it. */
            pa_stream_drop(s);
        }
        nbytes -= frag;
    }
    if (total == 0)
        return;

    if (audio->debug) {
        guint i;
        for (i = 0; i < total / framesize * audio->channels; i++)
            audio->peak = MAX(audio->peak, ABS((int)audio->buffer[i]));
    }

    if (volume != 1.) {
        guint i;
        for (i = 0; i < total / framesize * audio->channels; i++)
            audio->buffer[i] = (int16_t)CLAMP(audio->buffer[i] * volume,
                                              INT16_MIN, INT16_MAX);
    }

    /* If the sink is suspended (e.g. unplugged), just drop the chunk. */
    if (!audio->playback_ready)
        return;

    error = pa_stream_write(audio->playback, audio->buffer, total, NULL, 0,
                            PA_SEEK_RELATIVE);
    if (error < 0) {
        g_mutex_lock(&audio->lock);
        if (!audio->stop)
            g_warning("PulseAudio write failed: %s", pa_strerror(error));
        g_mutex_unlock(&audio->lock);
    } else if (audio->debug) {
        gint64 now = g_get_monotonic_time();
        pa_usec_t latency;
        int negative;

        audio->bytes += total;
        if (now - audio->last_report >= G_USEC_PER_SEC) {
            if (pa_stream_get_latency(audio->playback, &latency, &negative) >= 0)
                printf("PulseAudio: audio bridge: %.1f KiB/s, peak %.1f%%, "
                       "latency %.0f ms\n",
                       audio->bytes * G_USEC_PER_SEC /
                       (1024. * (now - audio->last_report)),
                       audio->peak * 100. / INT16_MAX,
                       (double)latency / 1000.);
            audio->bytes = 0;
            audio->peak = 0;
            audio->last_report = now;
        }
    }
}

/*
 * The worker thread only drives the PA mainloop; all data movement
 * happens in the read callback, so nothing blocks waiting on the sink.
 */
static gpointer audio_thread(gpointer user_data)
{
    struct cam_audio *audio = user_data;
    int ret;

    while (!audio->stop &&
           pa_mainloop_iterate(audio->mainloop, TRUE, &ret) >= 0)
        ;

    return NULL;
}

static const pa_buffer_attr low_latency_attr = {
    .tlength    = 0,
    .fragsize   = 10 * 1024,
    .minreq     = 0,

    /* Confine latency inside a limit considering 30 fps */
    .prebuf     = 48000 * 4 / 30,
    .maxlength  = 48000 * 4 / 15,
};

gboolean cam_audio_start(cam_t *cam)
{
    struct cam_audio *audio;
    pa_sample_spec sample_spec = {
        .format = PA_SAMPLE_S16LE,
        .rate = 48000,
        .channels = 2,
    };
    pa_mainloop *mainloop;
    pa_context *context;
    pa_stream *capture, *playback;
    pa_context_state_t state;
    gchar *source, *sink;
    gint card;
    int ret;

    if (cam->audio)
        return TRUE;

    card = get_audio_card(cam->video_dev, cam->debug);
    if (card < 0) {
        g_warning("No audio device is associated with %s", cam->video_dev);
        return FALSE;
    }
    source = get_audio_source(card, cam->debug);
    if (!source) {
        g_warning("No PulseAudio source is associated with %s", cam->video_dev);
        return FALSE;
    }
    sink = get_default_sink(cam->debug);
    if (!sink) {
        g_warning("No PulseAudio playback sink is available");
        g_free(source);
        return FALSE;
    }

    mainloop = pa_mainloop_new();
    if (!mainloop) {
        g_free(source);
        g_free(sink);
        return FALSE;
    }
    context = pa_context_new(pa_mainloop_get_api(mainloop), "Camorama");
    if (!context)
        goto free_mainloop;
    if (pa_context_connect(context, NULL, PA_CONTEXT_NOFLAGS, NULL) < 0)
        goto free_context;

    do {
        if (pa_mainloop_iterate(mainloop, TRUE, &ret) < 0)
            goto disconnect;
        state = pa_context_get_state(context);
    } while (state != PA_CONTEXT_READY &&
             state != PA_CONTEXT_FAILED && state != PA_CONTEXT_TERMINATED);
    if (state != PA_CONTEXT_READY)
        goto disconnect;

    capture = pa_stream_new(context, "Camorama capture", &sample_spec, NULL);
    if (!capture)
        goto disconnect;
    playback = pa_stream_new(context, "Camorama playback", &sample_spec, NULL);
    if (!playback) {
        pa_stream_unref(capture);
        goto disconnect;
    }

    audio = g_new0(struct cam_audio, 1);
    audio->mainloop = mainloop;
    audio->context = context;
    audio->channels = sample_spec.channels;
    audio->buffer = g_malloc((gsize)AUDIO_BUFFER_FRAMES *
                            sample_spec.channels * sizeof(int16_t));
    if (!audio->buffer) {
        pa_stream_unref(playback);
        pa_stream_unref(capture);
        g_free(audio);
        goto disconnect;
    }
    audio->capture = capture;
    audio->playback = playback;

    g_mutex_init(&audio->lock);
    audio->debug = cam->debug;
    audio->volume = cam->audio_volume;
    audio->last_report = g_get_monotonic_time();
    if (audio->debug)
        printf("PulseAudio: starting audio bridge (low latency)\n");

    pa_stream_set_state_callback(capture, stream_state_cb, audio);
    pa_stream_set_state_callback(playback, stream_state_cb, audio);
    pa_stream_set_suspended_callback(capture, stream_suspended_cb, audio);
    pa_stream_set_suspended_callback(playback, stream_suspended_cb, audio);
    pa_stream_set_read_callback(capture, capture_read_cb, audio);

    ret = pa_stream_connect_playback(playback, sink, &low_latency_attr,
                                     PA_STREAM_AUTO_TIMING_UPDATE |
                                     PA_STREAM_INTERPOLATE_TIMING,
                                     NULL, NULL);
    if (ret < 0) {
        g_warning("Could not play to PulseAudio: %s", pa_strerror(ret));
        goto cleanup;
    }
    ret = pa_stream_connect_record(capture, source, &low_latency_attr, 0);
    if (ret < 0) {
        g_warning("Could not record from PulseAudio: %s", pa_strerror(ret));
        goto cleanup;
    }

    g_free(source);
    g_free(sink);

    audio->thread = g_thread_new("camorama-audio", audio_thread, audio);
    cam->audio = audio;
    return TRUE;

cleanup:
    g_free(source);
    g_free(sink);
    pa_stream_unref(playback);
    pa_stream_unref(capture);
    g_free(audio->buffer);
    g_free(audio);
disconnect:
    pa_context_disconnect(context);
free_context:
    pa_context_unref(context);
free_mainloop:
    pa_mainloop_free(mainloop);
    return FALSE;
}

void cam_audio_stop(cam_t *cam)
{
    struct cam_audio *audio = cam->audio;

    if (!audio)
        return;

    if (audio->debug)
        printf("PulseAudio: stopping audio bridge\n");

    g_mutex_lock(&audio->lock);
    audio->stop = TRUE;
    g_mutex_unlock(&audio->lock);

    /* Drop whatever is still queued so nothing plays after stop. */
    pa_stream_flush(audio->capture, NULL, NULL);
    pa_stream_flush(audio->playback, NULL, NULL);

    pa_mainloop_quit(audio->mainloop, 0);
    g_thread_join(audio->thread);

    pa_stream_disconnect(audio->capture);
    pa_stream_unref(audio->capture);
    pa_stream_disconnect(audio->playback);
    pa_stream_unref(audio->playback);
    pa_context_disconnect(audio->context);
    pa_context_unref(audio->context);
    pa_mainloop_free(audio->mainloop);
    g_free(audio->buffer);
    g_free(audio);
    cam->audio = NULL;
}

void cam_audio_enable(GtkWidget *button, cam_t *cam)
{
    if (!cam->audio_available)
        return;
#if GTK_MAJOR_VERSION >= 4
    cam->audio_enabled = gtk_check_button_get_active(GTK_CHECK_BUTTON(button));
#else
    cam->audio_enabled = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(button));
#endif
    if (cam->audio_enabled && !cam_audio_start(cam)) {
        cam->audio_enabled = FALSE;
#if GTK_MAJOR_VERSION >= 4
        gtk_check_button_set_active(GTK_CHECK_BUTTON(button), FALSE);
#else
        gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(button), FALSE);
#endif
    }
    if (!cam->audio_enabled)
        cam_audio_stop(cam);
    if (cam->audio_volume_widget)
        gtk_widget_set_sensitive(cam->audio_volume_widget,
                                 cam->audio_enabled && cam->audio_available);
    g_settings_set_boolean(cam->gc, CAM_SETTINGS_AUDIO, cam->audio_enabled);
}

void cam_audio_volume(GtkRange *range, cam_t *cam)
{
    cam->audio_volume = CLAMP(gtk_range_get_value(range) / 100., 0., 1.);
    if (cam->audio) {
        g_mutex_lock(&cam->audio->lock);
        cam->audio->volume = cam->audio_volume;
        g_mutex_unlock(&cam->audio->lock);
    }
    if (cam->audio_volume_available)
        g_settings_set_double(cam->gc, CAM_SETTINGS_AUDIO_VOLUME,
                              cam->audio_volume);
}
