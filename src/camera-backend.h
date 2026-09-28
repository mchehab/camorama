#ifndef CAMORAMA_CAMERA_BACKEND_H
#define CAMORAMA_CAMERA_BACKEND_H


#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>
#include <gio/gio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <linux/types.h>
#include <linux/videodev2.h>
#include <signal.h>
#include <png.h>

#include <gtk/gtk.h>

#include "camorama-filter-chain.h"

typedef enum {
    PICMAX = 0,
    PICMIN = 1,
    PICHALF = 2
} CamoImageSize;

enum {
    JPEG = 0,
    PNG = 1,
    PPM = 2
};

struct buffer_start_len {
    void *start;
    size_t length;
};

struct resolutions {
    unsigned int pixformat;
    unsigned int x, y;
    unsigned int depth;
    float max_fps;
    int order;
};

struct colorspace_parms {
    enum v4l2_colorspace     colorspace;
    enum v4l2_xfer_func      xfer_func;
    enum v4l2_ycbcr_encoding ycbcr_enc;
    enum v4l2_quantization   quantization;
};

typedef struct  {
    char *name;
    gint64 value;
} video_control_menu_t;

struct camera;
struct cam_audio;

struct cam_display_buffer {
    unsigned char *data;
    size_t capacity;
    unsigned int width, height, rowstride;
    guint64 generation, sequence;
};

enum cam_display_state {
    CAM_DISPLAY_EMPTY,
    CAM_DISPLAY_READY,
    CAM_DISPLAY_OWNED
};

typedef struct video_controls {
    char *name;
    char *group;

    enum v4l2_ctrl_type type;
    guint32 id;
    gint32 min, max, def;
    gint32 step;

    unsigned int menu_size;
    video_control_menu_t *menu;

    struct camera *cam;
    GtkWidget *widget;            /* Owned by the controls window */

    void *next;

} video_controls_t;

struct camera_backend;

typedef struct libcamera_bridge libcamera_bridge_t;

typedef struct camera {
    int dev;
    unsigned int width, height;
    int bpp;
    float scale;
    CamoImageSize size;
    char name[32];

    video_controls_t *controls;

    int contrast, brightness, whiteness, colour, hue, zoom;
    guint32 zoom_cid;
    unsigned int bytesperline, sizeimage;
    unsigned int pixformat;
    gboolean force_pixformat;
    unsigned int requested_pixformat;
    int input;
    gint frame_number;

    int n_threads;

    GMutex remote_save_mutex;      /* Protects n_threads */
    GMutex control_win_mutex;      /* Protects controls_window */
    GMutex display_mutex;          /* Display roles and notification state */
    int stream_wakeup[2];

    unsigned int min_width, min_height, max_width, max_height;
    struct colorspace_parms colorspc;

    unsigned int n_res;
    struct resolutions *res;

    char *video_dev;
    unsigned char *image;
    gchar *capturefile, *rcapturefile;
    gchar *pixdir, *host, *proto, *rdir, *uri;
    int savetype, rsavetype;
    gchar *ts_string;
    gchar *date_format;
    gboolean debug, read, userptr, use_libv4l2, hidden;
    gboolean can_read, can_mmap;
    gboolean cap, rcap, acap, show_adjustments, show_effects, audio_enabled;
    gboolean audio_available;
    gboolean audio_volume_available;
    gdouble audio_volume;
    gboolean timestamp, rtimestamp, usedate, usestring;
    gboolean rtimefn, timefn;
    GtkWidget *da, *status, *audio_volume_widget;
    unsigned char *capture_input; /* Native bytes used by read() */
    guint timeout_id, timeout_fps_id, idle_id;
    GThread *stream_thread;
    gint stream_stop;
    guint display_source;
    struct cam_display_buffer display_buffers[2];
    guint display_write_index;    /* Worker-owned RGB slot */
    enum cam_display_state display_state;
    guint64 stream_generation, stream_sequence;
    guint32 timeout_interval;
    GSettings *gc;
    gboolean has_window_geometry_settings;
    gint window_width, window_height;
    GtkBuilder *xml;
    GdkPixbuf *pb;

    GtkApplication *app;
    GtkWidget *controls_window;
    guint screensaver_inhibit_cookie;
    struct cam_audio *audio;

    CamoramaFilterChain *filter_chain;

    gboolean rdir_ok;
    GFile *rdir_file;
    GMountOperation *rdir_mop;

    /* Buffer handling - should be used only inside v4l.c */
    struct v4l2_requestbuffers req;
    unsigned int n_buffers;
    struct {
        void *start;
        size_t length;
    } *buffers;

    /* Stateful decoder used by compressed image formats. */
    struct img_ffmpeg_data *converter;

    /* Selected camera operations; NULL selects the default V4L backend. */
    const struct camera_backend *backend;

    /* Opaque state owned by the libcamera backend. */
    libcamera_bridge_t *libcamera;
} cam_t;

int cam_ioctl(cam_t *cam, unsigned long cmd, void *arg);
/* Optional syscall backend, primarily for hardware-independent unit tests. */
struct cam_v4l_ops {
    int (*open)(const char *path, int flags);
    int (*close)(int fd);
    int (*read)(cam_t *cam, void *buffer, size_t size);
    int (*ioctl)(int fd, unsigned long cmd, void *arg);
};
void cam_set_v4l_ops(const struct cam_v4l_ops *ops);
int cam_open(cam_t *cam, int oflag);
int cam_close(cam_t *cam);
unsigned char *cam_read(cam_t *cam, unsigned char *display_data);
int cam_cancel_read(cam_t *cam);
int cam_query_controls(cam_t *cam);
void cam_free_controls(cam_t *cam);
video_controls_t *cam_find_control_per_id(cam_t *cam, guint32 id);
int cam_set_control(cam_t *cam, guint32 id, void *value);
int cam_get_control(cam_t *cam, guint32 id, void *value);
int camera_cap(cam_t *cam);
int print_cam(cam_t *cam);
GArray *cam_get_frame_intervals(cam_t *cam);
gboolean cam_set_frame_interval(cam_t *cam,
                                const struct v4l2_fract *interval);
gboolean cam_get_frame_interval(cam_t *cam,
                                struct v4l2_fract *interval);
int cam_set_max_fps(cam_t *cam);
int get_pic_info(cam_t *cam);
int get_win_info(cam_t *cam);
int try_set_win_info(cam_t *cam, unsigned int pixformat,
                     unsigned int *x, unsigned int *y);
int set_win_info(cam_t *cam);
int get_supported_resolutions(cam_t *cam, gboolean all_supported);
int start_streaming(cam_t *cam);
int stop_streaming(cam_t *cam);

struct camera_backend {
    const char *name;
    gboolean is_libcamera;
    int (*open)(cam_t *cam, int oflag);
    int (*close)(cam_t *cam);
    unsigned char *(*read)(cam_t *cam, unsigned char *output);
    void (*cancel_read)(cam_t *cam);
    int (*query_controls)(cam_t *cam);
    int (*set_control)(cam_t *cam, guint32 id, void *value);
    int (*get_control)(cam_t *cam, guint32 id, void *value);
    GArray *(*get_frame_intervals)(cam_t *cam);
    gboolean (*set_frame_interval)(cam_t *cam,
                                   const struct v4l2_fract *interval);
    gboolean (*get_frame_interval)(cam_t *cam,
                                   struct v4l2_fract *interval);
    int (*camera_cap)(cam_t *cam);
    void (*get_pic_info)(cam_t *cam);
    void (*get_win_info)(cam_t *cam);
    void (*try_set_win_info)(cam_t *cam, unsigned int pixformat,
                             unsigned int *width, unsigned int *height);
    void (*set_win_info)(cam_t *cam);
    void (*get_supported_resolutions)(cam_t *cam, gboolean all_supported);
    void (*print_cam)(cam_t *cam);
    void (*start_streaming)(cam_t *cam);
    void (*stop_streaming)(cam_t *cam);
};

extern const struct camera_backend v4l_camera_backend;

void camera_backend_set(cam_t *cam, const struct camera_backend *backend);
void camera_backend_select(cam_t *cam, const struct camera_backend *backend);
gboolean camera_backend_is_libcamera(const cam_t *cam);
const char *camera_backend_name(const cam_t *cam);

#endif /* CAMORAMA_CAMERA_BACKEND_H */
