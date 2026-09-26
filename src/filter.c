#include <config.h>

#include <glib/gi18n.h>

#include "filter.h"
#include "camera-backend.h"

gchar const *camorama_filter_get_name(CamoramaFilter * self)
{
    gchar const *name = CAMORAMA_FILTER_GET_CLASS(self)->name;

    g_return_val_if_fail(name, G_OBJECT_TYPE_NAME(self));
    return _(name);
}

void
camorama_filter_apply(CamoramaFilter *self, guchar *image, gint width,
                      gint height, gint depth)
{
    g_return_if_fail(CAMORAMA_FILTER_GET_CLASS(self)->filter);

    CAMORAMA_FILTER_GET_CLASS(self)->filter(self, image, width, height,
                                            depth);
}

void camorama_filter_show(CamoramaFilter *self, gpointer user_data)
{
    if (!CAMORAMA_FILTER_GET_CLASS(self)->show)
        return;

    if (CAMORAMA_FILTER_GET_CLASS(self)->showed)
        return;

    CAMORAMA_FILTER_GET_CLASS(self)->showed--;

    CAMORAMA_FILTER_GET_CLASS(self)->show(self, user_data);
}

void
camorama_filter_hide(CamoramaFilter *self)
{
    if (!CAMORAMA_FILTER_GET_CLASS(self)->hide)
        return;

    if (!(CAMORAMA_FILTER_GET_CLASS(self)->showed))
        return;

    CAMORAMA_FILTER_GET_CLASS(self)->showed--;

    CAMORAMA_FILTER_GET_CLASS(self)->hide(self);
}

/* GType stuff ifor CamoramaFilter */
G_DEFINE_ABSTRACT_TYPE(CamoramaFilter, camorama_filter, G_TYPE_OBJECT);

static void camorama_filter_init(CamoramaFilter *)
{
}

static void camorama_filter_class_init(CamoramaFilterClass *)
{
}

/* GType stuff for CamoramaFilterColor */
typedef struct _CamoramaFilter CamoramaFilterColor;
typedef struct _CamoramaFilterClass CamoramaFilterColorClass;

/* GType stuff for CamoramaFilterInvert */
typedef struct _CamoramaFilter CamoramaFilterInvert;
typedef struct _CamoramaFilterClass CamoramaFilterInvertClass;

G_DEFINE_TYPE(CamoramaFilterInvert, camorama_filter_invert,
              CAMORAMA_TYPE_FILTER);

static void camorama_filter_invert_init(CamoramaFilterInvert *)
{
}

static void
camorama_filter_invert_filter(void *, guchar *image, int x, int y,
                              int depth)
{
    int i;

    for (i = 0; i < x * y * depth; i++) {
        image[i] = 255 - image[i];
    }
}

static void
camorama_filter_invert_class_init(CamoramaFilterClass *self_class)
{
    self_class->filter = camorama_filter_invert_filter;
    self_class->name = _("Invert");
}

/* GType stuff for CamoramaFilterThreshold */
typedef struct _CamoramaFilterThreshold {
    CamoramaFilter base_instance;
    gint threshold;
} CamoramaFilterThreshold;
typedef struct _CamoramaFilterClass CamoramaFilterThresholdClass;

G_DEFINE_TYPE(CamoramaFilterThreshold, camorama_filter_threshold,
              CAMORAMA_TYPE_FILTER);

static void camorama_filter_threshold_init(CamoramaFilterThreshold *self)
{
    self->threshold = 127;
}

static void threshold_change(GtkScale *sc1, CamoramaFilterThreshold *self)
{

    self->threshold = gtk_range_get_value((GtkRange *) sc1);
}

static void camorama_filter_threshold_show(void *filter,
                                           gpointer data)
{
    CamoramaFilterThreshold *self = filter;
    cam_t *cam = data;

    CAMORAMA_FILTER_GET_CLASS(self)->data = data;

    gtk_widget_set_visible(GTK_WIDGET(gtk_builder_get_object(cam->xml,
                                                      "threshold_icon")), TRUE);;
    gtk_widget_set_visible(GTK_WIDGET(gtk_builder_get_object(cam->xml,
                                                      "threshold_label")), TRUE);;
    gtk_widget_set_visible(GTK_WIDGET(gtk_builder_get_object(cam->xml,
                                                      "threshold_slider")), TRUE);;

    gtk_range_set_value((GtkRange *)GTK_WIDGET(gtk_builder_get_object(cam->xml, "threshold_slider")),
                        self->threshold);

    g_signal_connect(gtk_builder_get_object(cam->xml, "threshold_slider"),
                         "value-changed", G_CALLBACK(threshold_change), self);
}

static void camorama_filter_threshold_hide(void *filter)
{
    CamoramaFilterThreshold *self = filter;

    cam_t *cam = CAMORAMA_FILTER_GET_CLASS(self)->data;
    gtk_widget_set_visible(GTK_WIDGET(gtk_builder_get_object(cam->xml,
                                                     "threshold_icon")), FALSE);;
    gtk_widget_set_visible(GTK_WIDGET(gtk_builder_get_object(cam->xml,
                                                     "threshold_slider")), FALSE);;
    gtk_widget_set_visible(GTK_WIDGET(gtk_builder_get_object(cam->xml,
                                                     "threshold_label")), FALSE);;
}

static void
camorama_filter_threshold_filter(void *filter, guchar *image, int x,
                                 int y, int)
{
    CamoramaFilterThreshold *self = filter;
    int i;

    for (i = 0; i < x * y; i++) {
        if ((image[0] + image[1] + image[2]) > (self->threshold * 3)) {
            image[0] = 255;
            image[1] = 255;
            image[2] = 255;
        } else {
            image[0] = 0;
            image[1] = 0;
            image[2] = 0;
        }
        image += 3;
    }
}

static void
camorama_filter_threshold_class_init(CamoramaFilterThresholdClass *
                                     self_class)
{
    self_class->show = camorama_filter_threshold_show;
    self_class->hide = camorama_filter_threshold_hide;
    self_class->filter = camorama_filter_threshold_filter;

    self_class->name = _("Threshold (Overall)");
}

/* GType stuff for CamoramaFilterThresholdChannel */
typedef struct _CamoramaFilterThreshold CamoramaFilterThresholdChannel;
typedef struct _CamoramaFilterClass CamoramaFilterThresholdChannelClass;

G_DEFINE_TYPE(CamoramaFilterThresholdChannel,
              camorama_filter_threshold_channel, CAMORAMA_TYPE_FILTER);

static void
camorama_filter_threshold_channel_init(CamoramaFilterThresholdChannel *
                                       self)
{
    self->threshold = 127;
}

static void ch_threshold_change(GtkScale *sc1, CamoramaFilterThreshold *self)
{

    self->threshold = gtk_range_get_value((GtkRange *) sc1);
}

static void camorama_filter_threshold_channel_show(void *filter,
                                           gpointer data)
{
    CamoramaFilterThreshold *self = filter;
    cam_t *cam = data;

    CAMORAMA_FILTER_GET_CLASS(self)->data = data;

    gtk_widget_set_visible(GTK_WIDGET(gtk_builder_get_object(cam->xml,
                                                      "ch_threshold_icon")), TRUE);;
    gtk_widget_set_visible(GTK_WIDGET(gtk_builder_get_object(cam->xml,
                                                      "ch_threshold_label")), TRUE);;
    gtk_widget_set_visible(GTK_WIDGET(gtk_builder_get_object(cam->xml,
                                                      "ch_threshold_slider")), TRUE);;

    gtk_range_set_value((GtkRange *)GTK_WIDGET(gtk_builder_get_object(cam->xml, "ch_threshold_slider")),
                        self->threshold);

    g_signal_connect(gtk_builder_get_object(cam->xml, "ch_threshold_slider"),
                         "value-changed", G_CALLBACK(ch_threshold_change), self);
}

static void camorama_filter_threshold_channel_hide(void *filter)
{
    CamoramaFilterThreshold *self = filter;

    cam_t *cam = CAMORAMA_FILTER_GET_CLASS(self)->data;
    gtk_widget_set_visible(GTK_WIDGET(gtk_builder_get_object(cam->xml,
                                                     "ch_threshold_icon")), FALSE);;
    gtk_widget_set_visible(GTK_WIDGET(gtk_builder_get_object(cam->xml,
                                                     "ch_threshold_slider")), FALSE);;
    gtk_widget_set_visible(GTK_WIDGET(gtk_builder_get_object(cam->xml,
                                                     "ch_threshold_label")), FALSE);;
}

static void
camorama_filter_threshold_channel_filter(void *filter,
                                         unsigned char *image, int x,
                                         int y, int)
{
    CamoramaFilterThresholdChannel *self = filter;
    int i;

    for (i = 0; i < x * y; i++) {
        if (image[0] > self->threshold) {
            image[0] = 255;
        } else {
            image[0] = 0;
        }
        if (image[1] > self->threshold) {
            image[1] = 255;
        } else {
            image[1] = 0;
        }
        if (image[2] > self->threshold) {
            image[2] = 255;
        } else {
            image[2] = 0;
        }
        image += 3;
    }
}

static void
camorama_filter_threshold_channel_class_init
(CamoramaFilterThresholdChannelClass *self_class)
{
    self_class->show = camorama_filter_threshold_channel_show;
    self_class->hide = camorama_filter_threshold_channel_hide;
    self_class->filter = camorama_filter_threshold_channel_filter;
    self_class->name = _("Threshold (Per Channel)");
}

/* GType stuff for CamoramaFilterWacky */
typedef struct _CamoramaFilter CamoramaFilterWacky;
typedef struct _CamoramaFilterClass CamoramaFilterWackyClass;

G_DEFINE_TYPE(CamoramaFilterWacky, camorama_filter_wacky,
              CAMORAMA_TYPE_FILTER);

static void camorama_filter_wacky_init(CamoramaFilterWacky *)
{
}

static void
camorama_filter_wacky_filter(void *, unsigned char *image, int x,
                             int y, int depth)
{
    int row, col, dx, dy;
    guchar *source = g_malloc_n((size_t)x * y, depth);

    memcpy(source, image, (size_t)x * y * depth);
    for (row = 1; row < y - 1; row++) {
        for (col = 1; col < x - 1; col++) {
            int offset = (row * x + col) * depth;
            int total = -20 * source[offset];
            int chan;

            for (dy = -1; dy <= 1; dy++) {
                for (dx = -1; dx <= 1; dx++) {
                    if (!dx && !dy)
                        continue;
                    total += (dx && dy ? 1 : 4) *
                             source[((row + dy) * x + col + dx) * depth];
                }
            }
            for (chan = 0; chan < depth; chan++)
                image[offset + chan] = CLAMP(total / 6, 0, 255);
        }
    }
    g_free(source);
}

static void
camorama_filter_wacky_class_init(CamoramaFilterWackyClass *self_class)
{
    self_class->filter = camorama_filter_wacky_filter;
    self_class->name = _("Wacky");
}

/* GType stuff for CamoramaFilterSmotth */
typedef struct _CamoramaFilter CamoramaFilterSmooth;
typedef struct _CamoramaFilterClass CamoramaFilterSmoothClass;

G_DEFINE_TYPE(CamoramaFilterSmooth, camorama_filter_smooth,
              CAMORAMA_TYPE_FILTER);

static void camorama_filter_smooth_init(CamoramaFilterSmooth *)
{
}

static void
camorama_filter_smooth_filter(void *, guchar *image, int x, int y,
                              int depth)
{
    int row, col, chan, dx, dy;
    guchar *source = g_malloc_n((size_t)x * y, depth);

    memcpy(source, image, (size_t)x * y * depth);

    for (row = 0; row < y; row++) {
        for (col = 0; col < x; col++) {
            for (chan = 0; chan < depth; chan++) {
                int total = 0, neighbours = 0;

                for (dy = -1; dy <= 1; dy++) {
                    if (row + dy < 0 || row + dy >= y)
                        continue;
                    for (dx = -1; dx <= 1; dx++) {
                        if ((!dx && !dy) || col + dx < 0 || col + dx >= x)
                            continue;
                        total += source[((row + dy) * x + col + dx) * depth + chan];
                        neighbours++;
                    }
                }
                if (neighbours)
                    image[(row * x + col) * depth + chan] = total / neighbours;
            }
        }
    }
    g_free(source);
}

static void
camorama_filter_smooth_class_init(CamoramaFilterSmoothClass *self_class)
{
    self_class->filter = camorama_filter_smooth_filter;
    self_class->name = _("Smooth");
}

/* GType for CamoramaFilterMono */
typedef struct _CamoramaFilter CamoramaFilterMono;
typedef struct _CamoramaFilterClass CamoramaFilterMonoClass;

G_DEFINE_TYPE(CamoramaFilterMono, camorama_filter_mono,
              CAMORAMA_TYPE_FILTER);

static void camorama_filter_mono_init(CamoramaFilterMono *)
{
}

static void
camorama_filter_mono_filter(void *, unsigned char *image, int x,
                            int y, int)
{
    int i;
    int total, avg;

    for (i = 0; i < x * y; i++) {
        total = image[0] + image[1] + image[2];
        avg = (int)(total / 3);

        image[0] = avg;
        image[1] = avg;
        image[2] = avg;
        image += 3;
    }
}

static void
camorama_filter_mono_class_init(CamoramaFilterMonoClass *self_class)
{
    self_class->filter = camorama_filter_mono_filter;
    self_class->name = _("Monochrome");
}

/* GType for CamoramaFilterMonoWeight */
typedef struct _CamoramaFilter CamoramaFilterMonoWeight;
typedef struct _CamoramaFilterClass CamoramaFilterMonoWeightClass;

G_DEFINE_TYPE(CamoramaFilterMonoWeight, camorama_filter_mono_weight,
              CAMORAMA_TYPE_FILTER);

static void
camorama_filter_mono_weight_init(CamoramaFilterMonoWeight *)
{
}

static void
camorama_filter_mono_weight_filter(void *, unsigned char *image,
                                   int x, int y, int)
{
    int i;
    int avg;

    for (i = 0; i < x * y; i++) {
        avg = (int)((image[0] * 0.2125) + (image[1] * 0.7154) +
                    (image[2] * 0.0721));

        image[0] = avg;
        image[1] = avg;
        image[2] = avg;
        image += 3;             /* bump to next triplet */
    }
}

static void
camorama_filter_mono_weight_class_init(CamoramaFilterMonoWeightClass *
                                       self_class)
{
    self_class->filter = camorama_filter_mono_weight_filter;
    self_class->name = _("Monochrome (Weight)");
}

/* GType stuff for CamoramaFilterSobel */
typedef struct _CamoramaFilter CamoramaFilterSobel;
typedef struct _CamoramaFilterClass CamoramaFilterSobelClass;

G_DEFINE_TYPE(CamoramaFilterSobel, camorama_filter_sobel,
              CAMORAMA_TYPE_FILTER);

static void camorama_filter_sobel_init(CamoramaFilterSobel *)
{
}

/* fix this at some point, very slow */
static void
camorama_filter_sobel_filter(void *, unsigned char *image, int x,
                             int y, int)
{
    int row, col, chan;
    int stride = x * 3;
    guchar *output = g_malloc0_n((size_t)x * y, 3);

    for (row = 1; row < y - 1; row++) {
        for (col = 1; col < x - 1; col++) {
            for (chan = 0; chan < 3; chan++) {
                int i = row * stride + col * 3 + chan;
                int dx = 2 * image[i + 3] + image[i - stride + 3] +
                         image[i + stride + 3] - 2 * image[i - 3] -
                         image[i - stride - 3] - image[i + stride - 3];
                int dy = image[i - stride - 3] + 2 * image[i - stride] +
                         image[i - stride + 3] - image[i + stride - 3] -
                         2 * image[i + stride] - image[i + stride + 3];

                output[i] = MIN((abs(dx) + abs(dy)) / 5.66, 255);
            }
        }
    }

    memcpy(image, output, (size_t)x * y * 3);
    g_free(output);
}

static void
camorama_filter_sobel_class_init(CamoramaFilterSobelClass *self_class)
{
    self_class->filter = camorama_filter_sobel_filter;
    // TRANSLATORS: http://en.wikipedia.org/wiki/Sobel
    self_class->name = _("Sobel");
}

/* general filter initialization */
void camorama_filters_init(void)
{
    camorama_filter_invert_get_type();
    camorama_filter_threshold_get_type();
    camorama_filter_threshold_channel_get_type();
    camorama_filter_mirror_get_type();
    camorama_filter_reichardt_get_type();
    camorama_filter_wacky_get_type();
    camorama_filter_smooth_get_type();
    camorama_filter_laplace_get_type();
    camorama_filter_mono_get_type();
    camorama_filter_mono_weight_get_type();
    camorama_filter_sobel_get_type();
}
