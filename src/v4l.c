#include <errno.h>
#include <glib/gi18n.h>
#include <stdlib.h>
#include <sys/select.h>
#include <time.h>
#include <config.h>
#include "v4l.h"
#include "camera-backend.h"
#include "img_convert.h"
#include "support.h"

extern int frame_number;

static int capture_buffers(cam_t *cam, unsigned char *outbuf);
static int capture_buffers_userptr(cam_t *cam, unsigned char *outbuf);

static gboolean is_format_supported(cam_t *cam, unsigned int pixformat)
{
    /*
     * As libv4l supports more formats and already selects the format
     * that provides the highest frame rate, use it, if not disabled.
     */
    if (cam->use_libv4l)
        return pixformat == V4L2_PIX_FMT_RGB24;

    return img_format_get(pixformat) != NULL;
}

static int v4l_cam_open(cam_t *cam, int oflag)
{
    if (cam->use_libv4l)
        return v4l2_open(cam->video_dev, oflag);
    else
        return open(cam->video_dev, oflag);
}

static int v4l_cam_close(cam_t *cam)
{
    if (cam->use_libv4l)
        return v4l2_close(cam->dev);
    else
        return close(cam->dev);
}

static unsigned char *v4l_cam_read(cam_t *cam)
{
    unsigned char *pic_buf = cam->pic_buf;
    int ret = 0;

    g_mutex_lock(&cam->pixbuf_mutex);
    if (cam->read) {
        if (cam->use_libv4l)
            ret = v4l2_read(cam->dev, cam->pic_buf,
                            (cam->width * cam->height * cam->bpp / 8));
        else {
            ret = read(cam->dev, cam->tmp, cam->sizeimage);
            if (ret > 0)
                ret = img_convert_to_rgb24(cam, cam->tmp, ret);
        }
    } else if (cam->userptr) {
        ret = capture_buffers_userptr(cam, cam->pic_buf);
    } else {
        ret = capture_buffers(cam, cam->pic_buf);
    }
    if (ret > 0)
        cam->frame_number++;
    else
        pic_buf = NULL;
    g_mutex_unlock(&cam->pixbuf_mutex);

    return pic_buf;
}

static int cam_ioctl(cam_t *cam, unsigned long cmd, void *arg)
{
    if (cam->use_libv4l)
        return v4l2_ioctl(cam->dev, cmd, arg);
    else
        return ioctl(cam->dev, cmd, arg);
}

video_controls_t *cam_find_control_per_id(cam_t *cam, guint32 id)
{
    video_controls_t *p = cam->controls;

    while (p) {
	if (p->id == id)
	    return p;
	p = p->next;
    }

    return NULL;
}

void cam_free_controls(cam_t *cam)
{
    guint32 i;

    if (cam->controls) {
	video_controls_t *p = cam->controls;
	while (p) {
	    free(p->name);
	    free(p->group);
	    if (p->menu) {
		for (i = 0; i < p->menu_size; i++)
		    free(p->menu[i].name);
		free(p->menu);
	    }
	    p = p->next;
	}
	free(cam->controls);
    }
    cam->controls = NULL;
}

static const char *cam_ctrl_type(uint32_t type)
{
    switch (type) {
    /* All controls below are available since, at least, Kernel 3.16 */
    case V4L2_CTRL_TYPE_INTEGER:
	return "int32";
    case V4L2_CTRL_TYPE_BOOLEAN:
	return "bool";
    case V4L2_CTRL_TYPE_MENU:
	return "menu";
    case V4L2_CTRL_TYPE_BUTTON:
	return "button";
    case V4L2_CTRL_TYPE_INTEGER64:
	return "int64";
    case V4L2_CTRL_TYPE_CTRL_CLASS:
	return "ctrl class";
    case V4L2_CTRL_TYPE_STRING:
	return "string";
    case V4L2_CTRL_TYPE_INTEGER_MENU:
	return "int menu";
    case V4L2_CTRL_TYPE_BITMASK:
	return "bitmask";
    case V4L2_CTRL_TYPE_U8:
	return "compound u8";
    case V4L2_CTRL_TYPE_U16:
	return "compound u16";
    case V4L2_CTRL_TYPE_U32:
	return "compound 32";

    /* Kernel v5.4 and upper, so test them to avoid build issues */
#ifdef V4L2_CTRL_TYPE_AREA
    case V4L2_CTRL_TYPE_AREA:
	return "area";
#endif
#ifdef V4L2_CTRL_TYPE_HDR10_CLL_INFO
    case V4L2_CTRL_TYPE_HDR10_CLL_INFO:
	return"HDR10 CLL_INFO";
#endif
#ifdef V4L2_CTRL_TYPE_HDR10_MASTERING_DISPLAY
    case V4L2_CTRL_TYPE_HDR10_MASTERING_DISPLAY:
	return"HDR10 MASTERING_DISPLAY";
#endif
#ifdef V4L2_CTRL_TYPE_H264_SPS
    case V4L2_CTRL_TYPE_H264_SPS:
	return"H264 SPS";
#endif
#ifdef V4L2_CTRL_TYPE_H264_PPS
    case V4L2_CTRL_TYPE_H264_PPS:
	return"H264 PPS";
#endif
#ifdef V4L2_CTRL_TYPE_H264_SCALING_MATRIX
    case V4L2_CTRL_TYPE_H264_SCALING_MATRIX:
	return"H264 SCALING_MATRIX";
#endif
#ifdef V4L2_CTRL_TYPE_H264_SLICE_PARAMS
    case V4L2_CTRL_TYPE_H264_SLICE_PARAMS:
	return"H264 SLICE_PARAMS";
#endif
#ifdef V4L2_CTRL_TYPE_H264_DECODE_PARAMS
    case V4L2_CTRL_TYPE_H264_DECODE_PARAMS:
	return"H264 DECODE_PARAMS";
#endif
#ifdef V4L2_CTRL_TYPE_H264_PRED_WEIGHTS
    case V4L2_CTRL_TYPE_H264_PRED_WEIGHTS:
	return"H264 PRED_WEIGHTS";
#endif
#ifdef V4L2_CTRL_TYPE_FWHT_PARAMS
    case V4L2_CTRL_TYPE_FWHT_PARAMS:
	return"FWHT PARAMS";
#endif
#ifdef V4L2_CTRL_TYPE_VP8_FRAME
    case V4L2_CTRL_TYPE_VP8_FRAME:
	return"VP8 FRAME";
#endif
#ifdef V4L2_CTRL_TYPE_MPEG2_QUANTISATION
    case V4L2_CTRL_TYPE_MPEG2_QUANTISATION:
	return"MPEG2 QUANTISATION";
#endif
#ifdef V4L2_CTRL_TYPE_MPEG2_SEQUENCE
    case V4L2_CTRL_TYPE_MPEG2_SEQUENCE:
	return"MPEG2 SEQUENCE";
#endif
#ifdef V4L2_CTRL_TYPE_MPEG2_PICTURE
    case V4L2_CTRL_TYPE_MPEG2_PICTURE:
	return"MPEG2 PICTURE";
#endif
#ifdef V4L2_CTRL_TYPE_VP9_COMPRESSED_HDR
    case V4L2_CTRL_TYPE_VP9_COMPRESSED_HDR:
	return"VP9 COMPRESSED_HDR";
#endif
#ifdef V4L2_CTRL_TYPE_VP9_FRAME
    case V4L2_CTRL_TYPE_VP9_FRAME:
	return"VP9 FRAME";
#endif
    default:
	return "unknown";
    }
}

static const char *cam_ctrl_class(uint32_t class)
{
    switch (class) {
    /* All controls below are available since, at least, Kernel 3.16 */
    case V4L2_CTRL_CLASS_USER:
	return "User controls";
#if defined(V4L2_CTRL_CLASS_CODEC)
    case V4L2_CTRL_CLASS_CODEC:
	return "Codec controls ";
#elif defined(V4L2_CTRL_CLASS_MPEG)
    case V4L2_CTRL_CLASS_MPEG:
	return "MPEG controls";
#endif
    case V4L2_CTRL_CLASS_CAMERA:
	return "Camera controls";
    case V4L2_CTRL_CLASS_FM_TX:
	return "FM Modulator controls";
    case V4L2_CTRL_CLASS_FLASH:
	return "Camera flash controls";
    case V4L2_CTRL_CLASS_JPEG:
	return "JPEG controls";
    case V4L2_CTRL_CLASS_IMAGE_SOURCE:
	return "Image source controls";
    case V4L2_CTRL_CLASS_IMAGE_PROC:
	return "Image processing controls";
    case V4L2_CTRL_CLASS_DV:
	return "Digital Video controls";
    case V4L2_CTRL_CLASS_FM_RX:
	return "FM Receiver controls";
    case V4L2_CTRL_CLASS_RF_TUNER:
	return "RF tuner controls";
    case V4L2_CTRL_CLASS_DETECT:
	return "Detection controls";
    /* Control classes for Kernel v5.10 and upper */
#ifdef V4L2_CTRL_CLASS_CODEC_STATELESS
    case V4L2_CTRL_CLASS_CODEC_STATELESS:
	return "Stateless Codec controls";
#endif
#ifdef V4L2_CTRL_CLASS_COLORIMETRY
    case V4L2_CTRL_CLASS_COLORIMETRY:
	return "Colorimetry controls";
#endif
    default:
	return "Other controls";
    }
}

// return values: 1: ignore, 0: added, -1: silently ignore
static int cam_add_ctrl(cam_t *cam,
                        struct v4l2_query_ext_ctrl *query,
                        video_controls_t **ptr)
{
    // Control is disabled, ignore it. Please notice that disabled controls
    // can be re-enabled. The right thing here would be to get those too,
    // and add a logic to
    if (query->flags & V4L2_CTRL_FLAG_DISABLED)
	return 1;

    /* Silently ignore control classes */
    if (query->type == V4L2_CTRL_TYPE_CTRL_CLASS)
	return -1;

    // There's not much sense on displaying permanent read-only controls
    if (query->flags & V4L2_CTRL_FLAG_READ_ONLY)
	return 1;

    // Allocate a new element on the linked list
    if (!cam->controls) {
	*ptr	      = g_new0(video_controls_t, 1);
	cam->controls = (void *)*ptr;
    } else {
	(*ptr)->next = g_new0(video_controls_t, 1);;
	*ptr	       = (*ptr)->next;
    }

    // Fill control data
    (*ptr)->id	  = query->id;
    (*ptr)->name  = strdup((const char *)query->name);
    (*ptr)->group = strdup(cam_ctrl_class(V4L2_CTRL_ID2CLASS(query->id)));
    (*ptr)->cam   = cam;
    #pragma GCC diagnostic push
    #pragma GCC diagnostic ignored "-Wswitch-enum"
    switch (query->type) {
    case V4L2_CTRL_TYPE_INTEGER:
	(*ptr)->type = V4L2_CTRL_TYPE_INTEGER;
	(*ptr)->min  = query->minimum;
	(*ptr)->max  = query->maximum;
	(*ptr)->def  = query->default_value;
	(*ptr)->step = query->step;
	break;
    case V4L2_CTRL_TYPE_INTEGER64:
	(*ptr)->type = V4L2_CTRL_TYPE_INTEGER64;
	(*ptr)->min  = query->minimum;
	(*ptr)->max  = query->maximum;
	(*ptr)->def  = query->default_value;
	(*ptr)->step = query->step;
	break;
    case V4L2_CTRL_TYPE_BOOLEAN:
	(*ptr)->type = V4L2_CTRL_TYPE_BOOLEAN;
	(*ptr)->def  = query->default_value;
	break;
    case V4L2_CTRL_TYPE_BUTTON:
	(*ptr)->type = V4L2_CTRL_TYPE_BUTTON;
	break;
    case V4L2_CTRL_TYPE_STRING:
	(*ptr)->type = V4L2_CTRL_TYPE_STRING;
	break;
    case V4L2_CTRL_TYPE_INTEGER_MENU:
    case V4L2_CTRL_TYPE_MENU: {
	struct v4l2_querymenu menu = { 0 };
	video_control_menu_t *first = NULL, *p;
	int n_menu = 0;

	menu.id = query->id;

	for (menu.index = query->minimum; menu.index <= query->maximum;
	     menu.index++) {
	    if (!ioctl(cam->dev, VIDIOC_QUERYMENU, &menu)) {
		first = g_realloc(first, (n_menu + 1) * sizeof(*(*ptr)->menu));

		p	 = &first[n_menu];
		p->value = menu.index;

		if (query->type == V4L2_CTRL_TYPE_INTEGER_MENU)
                   p->name = g_strdup_printf("%lli", menu.value);
		else
		    p->name = g_strdup((const char *)menu.name);

		n_menu++;
	    }
	}
	(*ptr)->menu	    = first;
	(*ptr)->menu_size   = n_menu;
	(*ptr)->min	    = query->minimum;
	(*ptr)->max	    = query->maximum;
	(*ptr)->def	    = query->default_value;
	(*ptr)->type	    = V4L2_CTRL_TYPE_MENU;
	break;
    }
    default:
	return (1);
    }
    #pragma GCC diagnostic pop

    return (0);
}

static int v4l_cam_query_controls(cam_t *cam)
{
    video_controls_t *ptr = NULL;
    struct v4l2_query_ext_ctrl query = { 0 };
    int ignore;
    const char *old_class = NULL;

    // Free controls list if not NULL
    cam_free_controls(cam);

    query.id = V4L2_CTRL_FLAG_NEXT_CTRL;
    while (!cam_ioctl(cam, VIDIOC_QUERY_EXT_CTRL, &query)) {
	ignore = cam_add_ctrl(cam, &query, &ptr);

	if (ignore >= 0 && cam->debug == TRUE) {
	    unsigned int i;
	    const char *class = cam_ctrl_class(V4L2_CTRL_ID2CLASS(query.id));
	    if (class != old_class)
		printf("Control class %s:\n", class);

	    printf("  %s (%s)%s\n", query.name, cam_ctrl_type(query.type),
		   ignore ? " - Ignored" : "");

	    for (i = 0; i < ptr->menu_size; i++)
		printf("    %" G_GINT64_FORMAT ": %s\n",
                       ptr->menu[i].value, ptr->menu[i].name);

	    old_class = class;
	}

	query.id |= V4L2_CTRL_FLAG_NEXT_CTRL;
    }

    return (0);
}

static int v4l_cam_set_control(cam_t *cam, guint32 id, void *value)
{
    struct v4l2_ext_controls ctrls;
    struct v4l2_ext_control c;
    video_controls_t *p;
    int ret;

    p = cam_find_control_per_id(cam, id);
    if (!p)
	return -1; // we have no such a control on the list

    memset(&ctrls, 0, sizeof(ctrls));
    ctrls.count = 1;
    /* Added on Kernel v4.4. Let's use it without testing */
    ctrls.which = V4L2_CTRL_ID2WHICH(p->id);
    ctrls.controls = &c;

    memset(&c, 0, sizeof(c));
    c.id = p->id;

    #pragma GCC diagnostic push
    #pragma GCC diagnostic ignored "-Wswitch-enum"
    switch (p->type) {
    case V4L2_CTRL_TYPE_INTEGER:
    case V4L2_CTRL_TYPE_BOOLEAN:
    case V4L2_CTRL_TYPE_BUTTON:
    case V4L2_CTRL_TYPE_MENU:
	c.value = *(int *)value;
	break;

    /* TODO: add support for V4L2_CTRL_TYPE_INTEGER64 */

    default:
	return -1;
    }
    #pragma GCC diagnostic pop

    ret = cam_ioctl(cam, VIDIOC_S_EXT_CTRLS, &ctrls);
    if (ret)
	printf("v4l2 set user control \"%s\" to %d returned %d\n", p->name,
               c.value, errno);

    if (cam->debug == TRUE)
        printf("  %s set to value %d\n", p->name, c.value);

    return 0;
}

static int v4l_cam_get_control(cam_t *cam, guint32 id, void *value)
{
    struct v4l2_ext_controls ctrls;
    struct v4l2_ext_control c;
    video_controls_t *p;
    int ret;

    p = cam_find_control_per_id(cam, id);
    if (!p)
	return -1;

    memset(&ctrls, 0, sizeof(ctrls));
    ctrls.count = 1;
    ctrls.which = V4L2_CTRL_ID2WHICH(p->id);
    ctrls.controls = &c;

    memset(&c, 0, sizeof(c));
    c.id = p->id;

    ret = cam_ioctl(cam, VIDIOC_G_EXT_CTRLS, &ctrls);
    if (ret) {
	printf("v4l2 get user control \"%s\" returned %d\n", p->name, ret);
	return -1;
    }

    #pragma GCC diagnostic push
    #pragma GCC diagnostic ignored "-Wswitch-enum"
    switch (p->type) {
    case V4L2_CTRL_TYPE_INTEGER:
        *(int *)value = c.value;
        if (cam->debug == TRUE)
            printf("  %s = %d (min: %d, max: %d, default: %d, step: %d)\n",
                   p->name, c.value, p->min, p->max, p->def, p->step);
	return (0);
    case V4L2_CTRL_TYPE_BOOLEAN:
    case V4L2_CTRL_TYPE_BUTTON:
    case V4L2_CTRL_TYPE_MENU:
	*(int *)value = c.value;
        if (cam->debug == TRUE)
            printf("  %s = %d\n", p->name, c.value);
	return (0);

    /* TODO: add support for V4L2_CTRL_TYPE_INTEGER64 */
    default:
	return -1;
    }
    #pragma GCC diagnostic pop
}

void print_cam(cam_t *cam)
{
    printf("\nCamera Info\n");
    printf("-----------\n");
    printf("device = %s, x = %d, y = %d\n", cam->video_dev, cam->width,
           cam->height);
    printf("bits per pixel = %d\n", cam->bpp);
    if (cam->width <= 0 || cam->height <= 0) {
        switch (cam->size) {
        case PICMAX:
            printf("size = PICMAX\n");
        break;
        case PICMIN:
            printf("size = PICMIN\n");
        break;
        case PICHALF:
        default:
            printf("size = PICHALF\n");
        break;
        }
    }
    printf("capture directory = %s, capture file = %s\n", cam->pixdir,
           cam->capturefile);
    printf("remote host = %s\n", cam->uri);
    if (strcmp(cam->ts_string, "Camorama!"))
        printf("timestamp = %s\n\n", cam->ts_string);
}

static void insert_resolution(cam_t *cam, unsigned int pixformat,
                              unsigned int x, unsigned int y,
                              float max_fps)
{
    const struct img_format *video_fmt;
    unsigned int i;

    try_set_win_info(cam, pixformat, &x, &y);

    if (cam->res) {
        for (i = 0; i < cam->n_res; i++) {
            if (cam->res[i].x == x && cam->res[i].y == y &&
                cam->res[i].pixformat == pixformat)
            return;
        }
    }

    cam->res = realloc(cam->res, (cam->n_res + 1) * sizeof(struct resolutions));

    cam->res[cam->n_res].pixformat = pixformat;
    cam->res[cam->n_res].x = x;
    cam->res[cam->n_res].y = y;
    cam->res[cam->n_res].max_fps = max_fps;

    video_fmt = img_format_get(pixformat);
    if (video_fmt) {
	int depth = video_fmt->depth;
        cam->res[cam->n_res].depth = depth;
	if (video_fmt->x_decimation)
	    depth /= video_fmt->x_decimation;
	if (video_fmt->y_decimation)
	    depth /= video_fmt->y_decimation;

        cam->res[cam->n_res].order = img_format_order(pixformat);

	if (video_fmt->x_decimation || video_fmt->y_decimation)
	    cam->res[cam->n_res].depth += depth << 1;
    } else {
        cam->res[cam->n_res].order = 99999;
    }

    if (cam->debug == TRUE)
        printf("  Resolution #%d: FOURCC: '%c%c%c%c' (%dx%d %.2f fps)\n",
               cam->n_res,
                pixformat & 0xff,
                (pixformat >> 8) & 0xff,
                (pixformat >> 16) & 0xff,
                pixformat >> 24,
                x, y, (double)max_fps);

    cam->n_res++;
}

static int sort_func(const void *__b, const void *__a)
{
    const struct resolutions *a = __a;
    const struct resolutions *b = __b;
    int r;

    r = (int)b->x - a->x;
    if (!r)
         r = (int)b->y - a->y;
    if (!r)
         r = (int)b->max_fps - a->max_fps;
    if (!r)
         r = (int)b->order - a->order;

    return r;
}

static float get_max_fps_discrete(cam_t *cam,
                                  struct v4l2_frmsizeenum *frmsize)
{
    struct v4l2_frmivalenum frmival = { 0 };
    float fps, max_fps = -1;

    frmival.width = frmsize->discrete.width;
    frmival.height = frmsize->discrete.height;
    frmival.pixel_format = frmsize->pixel_format;
    frmival.index = 0;

    for (frmival.index = 0;
         !cam_ioctl(cam, VIDIOC_ENUM_FRAMEINTERVALS, &frmival);
         frmival.index++) {
            fps = ((float)frmival.discrete.denominator)/frmival.discrete.numerator;
            if (fps > max_fps)
                max_fps = fps;
    }
    return max_fps;
}

static void v4l_get_supported_resolutions(cam_t *cam,
                                           gboolean all_supported)
{
    struct v4l2_fmtdesc fmtdesc = { 0 };
    struct v4l2_format fmt;
    struct v4l2_frmsizeenum frmsize = { 0 };
    gboolean has_framesizes = FALSE;
    unsigned int x, y, i;

    if (cam->n_res) {
        /* Free it, as the resolutions will be re-inserted */
        free(cam->res);
        cam->res = NULL;
        cam->n_res = 0;
    }
    fmtdesc.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

    for (fmtdesc.index = 0;
         !cam_ioctl(cam, VIDIOC_ENUM_FMT, &fmtdesc);
         fmtdesc.index++) {
        if (!all_supported) {
            if (cam->pixformat != fmtdesc.pixelformat)
                continue;

        } else {
            if (cam->debug == TRUE)
                printf("format index %d: FOURCC: '%c%c%c%c' (%08x)%s\n",
                       fmtdesc.index,
                       fmtdesc.pixelformat & 0xff,
                       (fmtdesc.pixelformat >> 8) & 0xff,
                       (fmtdesc.pixelformat >> 16) & 0xff,
                       fmtdesc.pixelformat >> 24,
                       fmtdesc.pixelformat,
                       fmtdesc.
                       flags & V4L2_FMT_FLAG_EMULATED ? " (emulated)" : "");

            memset(&fmt, 0, sizeof(fmt));
            fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
            fmt.fmt.pix.pixelformat = fmtdesc.pixelformat;
            fmt.fmt.pix.width = 48;
            fmt.fmt.pix.height = 32;

            if (!cam_ioctl(cam, VIDIOC_TRY_FMT, &fmt)) {
                if (fmt.fmt.pix.width < cam->min_width)
                        cam->min_width = fmt.fmt.pix.width;
                if (fmt.fmt.pix.height < cam->min_height)
                        cam->min_height = fmt.fmt.pix.height;
                if (cam->debug == TRUE)
                        printf("  MIN: %dx%d\n", fmt.fmt.pix.width,
                            fmt.fmt.pix.height);
            }

            memset(&fmt, 0, sizeof(fmt));
            fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
            fmt.fmt.pix.pixelformat = fmtdesc.pixelformat;
            fmt.fmt.pix.width = 100000;
            fmt.fmt.pix.height = 100000;

            if (!cam_ioctl(cam, VIDIOC_TRY_FMT, &fmt)) {
            if (fmt.fmt.pix.width > cam->max_width)
                    cam->max_width = fmt.fmt.pix.width;
            if (fmt.fmt.pix.height > cam->max_height)
                    cam->max_height = fmt.fmt.pix.height;
            if (cam->debug == TRUE)
                    printf("  MAX: %dx%d\n", fmt.fmt.pix.width,
                        fmt.fmt.pix.height);
            }
        }

        if (!is_format_supported(cam, fmtdesc.pixelformat))
            continue;

        frmsize.pixel_format = fmtdesc.pixelformat;
        frmsize.index = 0;

        while (!cam_ioctl(cam, VIDIOC_ENUM_FRAMESIZES, &frmsize)) {
            has_framesizes = TRUE;

            if (frmsize.type == V4L2_FRMSIZE_TYPE_DISCRETE) {
                    insert_resolution(cam, frmsize.pixel_format,
                                      frmsize.discrete.width,
                                      frmsize.discrete.height,
                                      get_max_fps_discrete(cam, &frmsize));
            } else if (frmsize.type == V4L2_FRMSIZE_TYPE_STEPWISE) {
                    for (i = 0; i <= 4; i++) {
                        x = frmsize.stepwise.min_width +
                            i * (frmsize.stepwise.max_width -
                                 frmsize.stepwise.min_width) / 4;
                        y = frmsize.stepwise.min_height +
                            i * (frmsize.stepwise.max_height -
                                 frmsize.stepwise.min_height) / 4;
                        insert_resolution(cam, frmsize.pixel_format, x, y, -1);
                    }
            }
            frmsize.index++;
        }

        if (!has_framesizes)
            insert_resolution(cam, fmtdesc.pixelformat,
                              fmt.fmt.pix.width, fmt.fmt.pix.height, -1);
    }

    if (!cam->res)
	return;

    qsort(cam->res, cam->n_res, sizeof(struct resolutions), sort_func);

    if (cam->debug == TRUE) {
	for (i = 0; i < cam->n_res; i++) {
	    printf("Resolution #%d: FOURCC: '%c%c%c%c' (%dx%d %.2f fps, %d depth)\n",
		   i,
		    cam->res[i].pixformat & 0xff,
		   (cam->res[i].pixformat >> 8) & 0xff,
		   (cam->res[i].pixformat >> 16) & 0xff,
		    cam->res[i].pixformat >> 24,
		   cam->res[i].x,
		   cam->res[i].y,
		   (double)cam->res[i].max_fps,
		   cam->res[i].depth);
	}
    }
}

static int v4l_camera_cap(cam_t *cam)
{
    char *msg;
    struct v4l2_capability vid_cap = { 0 };

    /* Query device capabilities */
    if (cam_ioctl(cam, VIDIOC_QUERYCAP, &vid_cap) == -1) {

        msg = g_strdup_printf(_("Could not connect to video device (%s).\n"
                                "Please check connection. Error: %d"),
                              cam->video_dev, errno);
        error_dialog(msg);
        g_free(msg);
        return 1;
    }

    if (!(vid_cap.device_caps & V4L2_CAP_VIDEO_CAPTURE)) {
        msg = g_strdup_printf(_("Device %s is not a video capture device."),
                              cam->video_dev);
        error_dialog(msg);
        g_free(msg);
        return 1;
    }

    if (!(vid_cap.device_caps & V4L2_CAP_STREAMING)) {
        printf("Device doesn't support streaming. Using read mode\n");

        cam->read = TRUE;
    } else {
        /* If MMAP is not supported, select USERPTR */
        memset(&cam->req, 0, sizeof(cam->req));
        cam->req.count = 2;
        cam->req.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        cam->req.memory = V4L2_MEMORY_MMAP;
        if (cam_ioctl(cam, VIDIOC_REQBUFS, &cam->req)) {
            printf("Device doesn't support mmap. Using userptr mode\n");
            cam_close(cam);
            cam->userptr = TRUE;
            cam->use_libv4l = FALSE;
            cam_open(cam, O_RDWR);
        } else {
            cam->req.count = 0;
            cam_ioctl(cam, VIDIOC_REQBUFS, &cam->req);
        }
    }

    /* Select input before querying resolutions */
    cam_ioctl(cam, VIDIOC_S_INPUT, &cam->input);

    /* Query supported resolutions */

    cam->rdir_ok = FALSE;
    cam->min_width = (unsigned)-1;
    cam->min_height = (unsigned)-1;
    cam->max_width = 0;
    cam->max_height = 0;

    get_supported_resolutions(cam, TRUE);

    /* Adjust camera resolution */

    if (cam->width > 0 && cam->height > 0) {
        if (cam->max_width < cam->width)
            cam->width = cam->max_width;
        else if (cam->min_width > cam->width)
            cam->width = cam->min_width;

        if (cam->max_height < cam->height)
            cam->height = cam->max_height;
        else if (cam->min_height > cam->height)
            cam->height = cam->min_height;
    } else {
        switch (cam->size) {
        case PICMAX:
            cam->width = cam->max_width;
            cam->height = cam->max_height;
        break;

        case PICMIN:
            cam->width = cam->min_width;
            cam->height = cam->min_height;
        break;

        case PICHALF:
            cam->width = cam->max_width / 2;
            cam->height = cam->max_height / 2;
        break;

        default:
            cam->width = cam->max_width / 2;
            cam->height = cam->max_height / 2;
        break;
        }
    }

    strncpy(cam->name, (const char *)vid_cap.card, sizeof(cam->name));
    cam->name[sizeof(cam->name) - 1] = '\0';

    if (cam->debug == TRUE) {
        printf("\nVIDIOC_QUERYCAP\n");
        printf("device name = %s\n", vid_cap.card);
        printf("device caps = 0x%08x\n", vid_cap.device_caps);
        printf("max width = %d\n", cam->max_width);
        printf("max height = %d\n", cam->max_height);
        printf("min width = %d\n", cam->min_width);
        printf("min height = %d\n", cam->min_height);
    }

    return 0;
}

static int v4l_get_zoom(cam_t *cam, int *value)
{
    int ret;

    cam->zoom_cid = V4L2_CID_ZOOM_ABSOLUTE;
    ret = cam_get_control(cam, cam->zoom_cid, value);
    if (!ret)
        return 0;

    cam->zoom_cid = V4L2_CID_ZOOM_RELATIVE;
    ret = cam_get_control(cam, cam->zoom_cid, value);
    if (!ret)
        return 0;

    cam->zoom_cid = V4L2_CID_ZOOM_CONTINUOUS;
    ret = cam_get_control(cam, cam->zoom_cid,value);
    if (!ret)
        return 0;

    cam->zoom_cid = 0;

    return -1;
}

static void v4l_get_pic_info(cam_t *cam)
{
    int i, ret;

    cam_query_controls(cam);

    if (cam->debug == TRUE)
        printf("\nVideo control settings:\n");

    ret = cam_get_control(cam, V4L2_CID_HUE, &i);
    if (!ret) {
        cam->hue = i;
    } else {
        cam->hue = -1;
    }
    ret = cam_get_control(cam, V4L2_CID_SATURATION, &i);
    if (!ret) {
        cam->colour = i;
    } else {
        cam->colour = -1;
    }
    ret = v4l_get_zoom(cam, &i);
    if (!ret) {
        cam->zoom = i;
    } else {
        cam->zoom = -1;
    }

    ret = cam_get_control(cam, V4L2_CID_CONTRAST, &i);
    if (!ret) {
        cam->contrast = i;
    } else {
        cam->contrast = -1;
    }
    ret = cam_get_control(cam, V4L2_CID_WHITENESS, &i);
    if (!ret) {
        cam->whiteness = i;
    } else {
        cam->whiteness = -1;
    }
    ret = cam_get_control(cam, V4L2_CID_BRIGHTNESS, &i);
    if (!ret) {
        cam->brightness = i;
    } else {
        cam->brightness = -1;
    }
}

static void v4l_get_win_info(cam_t *cam)
{
    gchar *msg;
    struct v4l2_format fmt = { 0 };

    fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

    if (cam_ioctl(cam, VIDIOC_G_FMT, &fmt)) {
        msg = g_strdup_printf(_("Could not connect to video device (%s).\nPlease check connection."),
                              cam->video_dev);
        error_dialog(msg);
        if (cam->debug == TRUE)
            g_message("VIDIOC_G_FMT  --  could not get picture info");

        return;
    }

    if (cam->debug == TRUE) {
        printf("\nVIDIOC_G_FMT\n");
        printf("format FOURCC: '%c%c%c%c' (%08x)\n",
               fmt.fmt.pix.pixelformat & 0xff,
               (fmt.fmt.pix.pixelformat >> 8) & 0xff,
               (fmt.fmt.pix.pixelformat >> 16) & 0xff,
               fmt.fmt.pix.pixelformat >> 24,
               fmt.fmt.pix.pixelformat);
        printf("x = %d\n", fmt.fmt.pix.width);
        printf("y = %d\n", fmt.fmt.pix.height);
        if (fmt.fmt.pix.bytesperline)
            printf("bytes/line = %d\n", fmt.fmt.pix.bytesperline);
    }
    img_get_colorspace_data(cam, &fmt);

    if (!fmt.fmt.pix.bytesperline) {
        if (fmt.fmt.pix.pixelformat == V4L2_PIX_FMT_YUV420)
            fmt.fmt.pix.bytesperline = fmt.fmt.pix.width * 2;
        else
            fmt.fmt.pix.bytesperline = fmt.fmt.pix.width * 3;
    }

    if (is_format_supported(cam, fmt.fmt.pix.pixelformat)) {
        cam->pixformat = fmt.fmt.pix.pixelformat;
        cam->bpp = ((fmt.fmt.pix.bytesperline << 3) + (fmt.fmt.pix.width - 1)) / fmt.fmt.pix.width;
        cam->width = fmt.fmt.pix.width;
        cam->height = fmt.fmt.pix.height;
        cam->bytesperline = fmt.fmt.pix.bytesperline;
        cam->sizeimage = fmt.fmt.pix.sizeimage;
    }
}

static void v4l_try_set_win_info(cam_t *cam, unsigned int pixformat,
                                 unsigned int *x, unsigned int *y)
{
    struct v4l2_format fmt;

    memset(&fmt, 0, sizeof(fmt));
    fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    fmt.fmt.pix.pixelformat = pixformat;
    fmt.fmt.pix.width = *x;
    fmt.fmt.pix.height = *y;
    if (!cam_ioctl(cam, VIDIOC_TRY_FMT, &fmt)) {
        *x = fmt.fmt.pix.width;
        *y = fmt.fmt.pix.height;
    }
}

static void v4l_set_win_info(cam_t *cam)
{
    struct v4l2_format fmt;
    gchar *msg;

    memset(&fmt, 0, sizeof(fmt));
    fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

    /* Get current settings, apply our changes and try the new setting */
    if (cam_ioctl(cam, VIDIOC_G_FMT, &fmt)) {
        if (cam->debug) {
            g_message("VIDIOC_G_FMT  --  could not get window info, exiting....");
        }
        msg = g_strdup_printf(_("Could not connect to video device (%s).\nPlease check connection."),
                              cam->video_dev);
        error_dialog(msg);
        g_free(msg);
        exit(0);
    }

    if (!cam->n_res) {
        msg = g_strdup_printf(_("Device video formats are incompatible with camorama."));
        error_dialog(msg);
        g_free(msg);
        exit(0);
    }

    cam->pixformat = cam->res[0].pixformat;

    fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    fmt.fmt.pix.pixelformat = cam->pixformat;
    fmt.fmt.pix.width = cam->width;
    fmt.fmt.pix.height = cam->height;
    if (cam_ioctl(cam, VIDIOC_S_FMT, &fmt)) {
        if (cam->debug)
            g_message("VIDIOC_S_FMT  --  could not set window info, exiting....");

        msg = g_strdup_printf(_("Could not set window info on video device (%s).\nPlease check connection."),
                              cam->video_dev);
        error_dialog(msg);
        g_free(msg);
        exit(0);
    }
    cam->pixformat = fmt.fmt.pix.pixelformat;

    /* Check if returned format is valid */
    if (!is_format_supported(cam, cam->pixformat)) {
        if (cam->debug) {
            g_message("VIDIOC_S_FMT  --  could not set format to %c%c%c%c (was set to %c%c%c%c instead), exiting....",
                      cam->pixformat & 0xff,
                      (cam->pixformat >> 8) & 0xff,
                      (cam->pixformat >> 16) & 0xff,
                      cam->pixformat >> 24,
                      fmt.fmt.pix.pixelformat & 0xff,
                      (fmt.fmt.pix.pixelformat >> 8) & 0xff,
                      (fmt.fmt.pix.pixelformat >> 16) & 0xff,
                      fmt.fmt.pix.pixelformat >> 24);
        }
        msg = g_strdup_printf(_("Could not set format to %c%c%c%c on video device (%s)."),
                              cam->pixformat & 0xff,
                              (cam->pixformat >> 8) & 0xff,
                              (cam->pixformat >> 16) & 0xff,
                              cam->pixformat >> 24,
                              cam->video_dev);
        error_dialog(msg);
        g_free(msg);
        exit(0);
    }

    /* Resolution may have changed. Store the retrieved one */
    cam->pixformat = fmt.fmt.pix.pixelformat;
    cam->bytesperline = fmt.fmt.pix.bytesperline;
    cam->sizeimage = fmt.fmt.pix.sizeimage;

    if (!fmt.fmt.pix.width) {
        g_message("VIDIOC_S_FMT  --  zero width returned!");

        msg = g_strdup_printf(_("Returned width is zero on video device (%s)!"),
                              cam->video_dev);
        error_dialog(msg);
        g_free(msg);
        exit(0);
    }
    cam->bpp = ((fmt.fmt.pix.bytesperline << 3) + (fmt.fmt.pix.width - 1)) / fmt.fmt.pix.width;

    cam->width = fmt.fmt.pix.width;
    cam->height = fmt.fmt.pix.height;
}

static void v4l_start_streaming(cam_t *cam)
{
    char *msg;
    unsigned int i;
    enum v4l2_buf_type type;
    struct v4l2_buffer buf;

    memset(&cam->req, 0, sizeof(cam->req));
    cam->req.count = 2;
    cam->req.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    cam->req.memory = V4L2_MEMORY_MMAP;
    if (cam_ioctl(cam, VIDIOC_REQBUFS, &cam->req)) {
        msg = g_strdup_printf(_("VIDIOC_REQBUFS  --  could not request buffers (%s), exiting...."),
                              cam->video_dev);
        error_dialog(msg);
        g_free(msg);
        exit(0);
    }

    cam->buffers = calloc(cam->req.count, sizeof(*cam->buffers));
    if (!cam->buffers) {
        msg = g_strdup_printf(_("could not allocate memory for video buffers, exiting...."));
        error_dialog(msg);
        g_free(msg);
        exit(0);
    }

    for (cam->n_buffers = 0;
         cam->n_buffers < cam->req.count;
         ++cam->n_buffers) {
        memset(&buf, 0, sizeof(buf));

        buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        buf.memory = V4L2_MEMORY_MMAP;
        buf.index = cam->n_buffers;

        if (cam_ioctl(cam, VIDIOC_QUERYBUF, &buf)) {
            msg = g_strdup_printf(_("VIDIOC_QUERYBUF  --  could not query buffers (%s), exiting...."),
                                  cam->video_dev);
            error_dialog(msg);
            g_free(msg);
            exit(0);
        }

        cam->buffers[cam->n_buffers].length = buf.length;
        cam->buffers[cam->n_buffers].start = v4l2_mmap(NULL, buf.length,
                                                       PROT_READ | PROT_WRITE,
                                                       MAP_SHARED,
                                                       cam->dev,
                                                       buf.m.offset);

        if (cam->buffers[cam->n_buffers].start == MAP_FAILED) {
            msg = g_strdup_printf(_("failed to memory map buffers (%s), exiting...."),
                                  cam->video_dev);
            error_dialog(msg);
            g_free(msg);
            exit(0);
        }
    }

    for (i = 0; i < cam->n_buffers; ++i) {
        memset(&buf, 0, sizeof(buf));
        buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        buf.memory = V4L2_MEMORY_MMAP;
        buf.index = i;
        if (cam_ioctl(cam, VIDIOC_QBUF, &buf)) {
            msg = g_strdup_printf(_("VIDIOC_QBUF  --  could not enqueue buffers (%s), exiting...."),
                                  cam->video_dev);
            error_dialog(msg);
            g_free(msg);
            exit(0);
        }
    }
    type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

    if (cam_ioctl(cam, VIDIOC_STREAMON, &type)) {
        msg = g_strdup_printf(_("failed to start streaming (%s), exiting...."),
                              cam->video_dev);
        error_dialog(msg);
        g_free(msg);
        exit(0);
    }
}

#define PAGE_SIZE 4096
#define PAGE_MASK (~(PAGE_SIZE - 1))
#define PAGE_ALIGN(x) ((typeof(x))(((unsigned long)(x) + PAGE_SIZE - 1) & PAGE_MASK))

void start_streaming_userptr(cam_t *cam)
{
    char *msg;
    enum v4l2_buf_type type;
    struct v4l2_buffer buf;

    memset(&cam->req, 0, sizeof(cam->req));
    cam->req.count = 2;
    cam->req.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    cam->req.memory = V4L2_MEMORY_USERPTR;
    if (cam_ioctl(cam, VIDIOC_REQBUFS, &cam->req)) {
        msg = g_strdup_printf(_("VIDIOC_REQBUFS  --  could not request buffers (%s), exiting...."),
                              cam->video_dev);
        error_dialog(msg);
        g_free(msg);
        exit(0);
    }

    cam->buffers = calloc(cam->req.count, sizeof(*cam->buffers));
    if (!cam->buffers) {
        msg = g_strdup_printf(_("could not allocate memory for video buffers, exiting...."));
        error_dialog(msg);
        g_free(msg);
        exit(0);
    }

    for (cam->n_buffers = 0;
         cam->n_buffers < cam->req.count;
         ++cam->n_buffers) {
        memset(&buf, 0, sizeof(buf));

        /*
         * The userptr buffer pages must not be used by anything else
         * so round up the size to pagesize; and align the pointer
         * returned by calloc to start at a page (which requires
         * allocating an extra page)
         */
        cam->buffers[cam->n_buffers].length = PAGE_ALIGN(cam->sizeimage);
        cam->buffers[cam->n_buffers].start = PAGE_ALIGN(
                calloc(1, cam->buffers[cam->n_buffers].length + PAGE_SIZE));

        if (!cam->buffers[cam->n_buffers].start) {
            msg = g_strdup_printf(_("could not allocate memory for video buffers, exiting...."));
            error_dialog(msg);
            g_free(msg);
            exit(0);
        }

        buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        buf.memory = V4L2_MEMORY_USERPTR;
        buf.index = cam->n_buffers;
        buf.length = cam->buffers[cam->n_buffers].length;
        buf.m.userptr = (unsigned long)cam->buffers[cam->n_buffers].start;

        if (cam_ioctl(cam, VIDIOC_QBUF, &buf)) {
            msg = g_strdup_printf(_("VIDIOC_QBUF  --  could not query buffers (%s), exiting...."),
                                  cam->video_dev);
            error_dialog(msg);
            g_free(msg);
            exit(0);
        }
    }

    type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

    if (cam_ioctl(cam, VIDIOC_STREAMON, &type)) {
        msg = g_strdup_printf(_("failed to start streaming (%s), exiting...."),
                              cam->video_dev);
        error_dialog(msg);
        g_free(msg);
        exit(0);
    }
}

static int capture_buffers(cam_t *cam, unsigned char *outbuf)
{
    char *msg;
    unsigned char *inbuf;
    int r;
    unsigned int y;
    fd_set fds;
    struct v4l2_buffer buf;
    struct timeval tv;

    do {
        FD_ZERO(&fds);
        FD_SET(cam->dev, &fds);

        /* Timeout. */
        tv.tv_sec = 2;
        tv.tv_usec = 0;

        r = select(cam->dev + 1, &fds, NULL, NULL, &tv);
    } while ((r == -1 && (errno == EINTR)));

    if (r == -1) {
        msg = g_strdup_printf(_("Timeout while waiting for frames (%s)"),
                              cam->video_dev);
        error_dialog(msg);
        g_free(msg);
        exit(0);
    }

    memset(&buf, 0, sizeof(buf));
    buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    buf.memory = V4L2_MEMORY_MMAP;
    cam_ioctl(cam, VIDIOC_DQBUF, &buf);

    inbuf = cam->buffers[buf.index].start;
    if (cam->use_libv4l) {
	    for (y = 0; y < cam->height; y++) {
		memcpy(outbuf, inbuf, cam->width * cam->bpp / 8);
		outbuf += cam->width * cam->bpp / 8;
		inbuf += cam->bytesperline;
	    }
        r = cam->width * cam->height * 3;
    } else {
	    r = img_convert_to_rgb24(cam, inbuf, buf.bytesused);
    }

    cam_ioctl(cam, VIDIOC_QBUF, &buf);
    return r;
}

static int capture_buffers_userptr(cam_t *cam, unsigned char *outbuf)
{
    char *msg;
    unsigned char *inbuf;
    int r;
    unsigned int y;
    fd_set fds;
    struct v4l2_buffer buf;
    struct timeval tv;

    do {
        FD_ZERO(&fds);
        FD_SET(cam->dev, &fds);

        /* Timeout. */
        tv.tv_sec = 2;
        tv.tv_usec = 0;

        r = select(cam->dev + 1, &fds, NULL, NULL, &tv);
    } while ((r == -1 && (errno == EINTR)));

    if (r == -1) {
        msg = g_strdup_printf(_("Timeout while waiting for frames (%s)"),
                              cam->video_dev);
        error_dialog(msg);
        g_free(msg);
        exit(0);
    }

    memset(&buf, 0, sizeof(buf));
    buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    buf.memory = V4L2_MEMORY_USERPTR;
    cam_ioctl(cam, VIDIOC_DQBUF, &buf);

    inbuf = cam->buffers[buf.index].start;
    if (cam->use_libv4l) {
	    for (y = 0; y < cam->height; y++) {
		memcpy(outbuf, inbuf, cam->width * cam->bpp / 8);
		outbuf += cam->width * cam->bpp / 8;
		inbuf += cam->bytesperline;
	    }
        r = cam->width * cam->height * 3;
    } else {
	    r = img_convert_to_rgb24(cam, inbuf, buf.bytesused);
    }

    cam_ioctl(cam, VIDIOC_QBUF, &buf);
    return r;
}

static void v4l_stop_streaming(cam_t *cam)
{
    char *msg;
    unsigned int i;
    int r;
    enum v4l2_buf_type type;
    fd_set fds, fderrs;
    struct v4l2_buffer buf;
    struct timeval tv;

    /* Dequeue all pending buffers */
    for (i = 0; i < cam->n_buffers; ++i) {
        FD_ZERO(&fds);
        FD_SET(cam->dev, &fds);
        FD_ZERO(&fderrs);
        FD_SET(cam->dev, &fderrs);

        /* Timeout. */
        tv.tv_sec = 2;
        tv.tv_usec = 0;

        r = select(cam->dev + 1, &fds, NULL, &fderrs, &tv);
        if (r == -1 && errno == EINTR)
            continue;

        if (r != -1) {
            memset(&buf, 0, sizeof(buf));
            buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
            buf.memory = V4L2_MEMORY_MMAP;
            if (cam_ioctl(cam, VIDIOC_DQBUF, &buf))
                break;
        }
    };

    /* Streams off */
    type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    if (cam_ioctl(cam, VIDIOC_STREAMOFF, &type)) {
        msg = g_strdup_printf(_("failed to stop streaming (%s), exiting...."),
                              cam->video_dev);
        error_dialog(msg);
        g_free(msg);
        exit(0);
    }

    /* Unmap buffers */
    for (i = 0; i < cam->n_buffers; ++i)
        v4l2_munmap(cam->buffers[i].start, cam->buffers[i].length);

    /* Free existing buffers */
    memset(&cam->req, 0, sizeof(cam->req));
    cam->req.count = 0;
    cam->req.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    cam->req.memory = V4L2_MEMORY_MMAP;
    cam_ioctl(cam, VIDIOC_REQBUFS, &cam->req);

    free(cam->buffers);
    cam->buffers = NULL;
}

void stop_streaming_userptr(cam_t *cam)
{
    char *msg;
    unsigned int i;
    int r;
    enum v4l2_buf_type type;
    fd_set fds, fderrs;
    struct v4l2_buffer buf;
    struct timeval tv;

    /* Dequeue all pending buffers */
    for (i = 0; i < cam->n_buffers; ++i) {
        FD_ZERO(&fds);
        FD_SET(cam->dev, &fds);
        FD_ZERO(&fderrs);
        FD_SET(cam->dev, &fderrs);

        /* Timeout. */
        tv.tv_sec = 2;
        tv.tv_usec = 0;

        r = select(cam->dev + 1, &fds, NULL, &fderrs, &tv);
        if (r == -1 && errno == EINTR)
            continue;

        if (r != -1) {
            memset(&buf, 0, sizeof(buf));
            buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
            buf.memory = V4L2_MEMORY_USERPTR;
            if (cam_ioctl(cam, VIDIOC_DQBUF, &buf))
                break;
        }
    };

    /* Streams off */
    type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    if (cam_ioctl(cam, VIDIOC_STREAMOFF, &type)) {
        msg = g_strdup_printf(_("failed to stop streaming (%s), exiting...."),
                              cam->video_dev);
        error_dialog(msg);
        g_free(msg);
        exit(0);
    }

    /* Unmap buffers */
    for (i = 0; i < cam->n_buffers; ++i)
        v4l2_munmap(cam->buffers[i].start, cam->buffers[i].length);

    /* Free existing buffers */
    memset(&cam->req, 0, sizeof(cam->req));
    cam->req.count = 0;
    cam->req.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    cam->req.memory = V4L2_MEMORY_MMAP;
    cam_ioctl(cam, VIDIOC_REQBUFS, &cam->req);

    free(cam->buffers);
    cam->buffers = NULL;
}

const camera_backend_t v4l_camera_backend = {
    .name = "Video4Linux",
    .open = v4l_cam_open,
    .close = v4l_cam_close,
    .read = v4l_cam_read,
    .query_controls = v4l_cam_query_controls,
    .set_control = v4l_cam_set_control,
    .get_control = v4l_cam_get_control,
    .camera_cap = v4l_camera_cap,
    .get_pic_info = v4l_get_pic_info,
    .get_win_info = v4l_get_win_info,
    .try_set_win_info = v4l_try_set_win_info,
    .set_win_info = v4l_set_win_info,
    .get_supported_resolutions = v4l_get_supported_resolutions,
    .start_streaming = v4l_start_streaming,
    .stop_streaming = v4l_stop_streaming,
};
