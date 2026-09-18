#ifndef IMG_CONFIG_H
#define IMG_CONFIG_H

#include <config.h>

#ifdef HAVE_FFMPEG
#  include <libavcodec/avcodec.h>
#  include <libavutil/error.h>
#  include <libavutil/pixfmt.h>
#  include <libswscale/swscale.h>

struct img_ffmpeg_data {
    AVCodecContext *codec;
    AVFrame *frame;
    AVPacket *packet;
    struct SwsContext *sws;
    unsigned int pixformat;
};

int img_ffmpeg_to_rgb24(struct img_ffmpeg_data **converter,
                        unsigned int pixformat, const unsigned char *input,
                        size_t input_size, unsigned char *output,
                        unsigned int width, unsigned int height);
void img_ffmpeg_free_converter(struct img_ffmpeg_data **converter);

#endif // HAVE_FFMPEG

#endif // IMG_CONFIG_H
