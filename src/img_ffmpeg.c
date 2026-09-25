#include <glib.h>
#include <linux/videodev2.h>
#include <string.h>

#include "img_ffmpeg.h"

static enum AVCodecID v4l2_to_ffmeg(unsigned int pixformat)
{
    switch (pixformat) {
    case V4L2_PIX_FMT_JPEG:
    case V4L2_PIX_FMT_MJPEG:
        return AV_CODEC_ID_MJPEG;
    case V4L2_PIX_FMT_H264:
        return AV_CODEC_ID_H264;
    default:
        return AV_CODEC_ID_NONE;
    }
}

static void img_ffmpeg_free(struct img_ffmpeg_data *converter)
{
    if (!converter)
        return;

    sws_freeContext(converter->sws);
    av_packet_free(&converter->packet);
    av_frame_free(&converter->frame);
    avcodec_free_context(&converter->codec);
    g_free(converter);
}

void img_ffmpeg_free_converter(struct img_ffmpeg_data **converter)
{
    if (!converter)
        return;

    img_ffmpeg_free(*converter);
    *converter = NULL;
}

static int img_ffmpeg_prepare(struct img_ffmpeg_data **converter,
                             unsigned int pixformat)
{
    struct img_ffmpeg_data *new_converter;
    enum AVCodecID codec_id;
    const AVCodec *codec;
    int ret;

    if (*converter && (*converter)->pixformat == pixformat)
        return 0;

    img_ffmpeg_free(*converter);
    *converter = NULL;

    codec_id = v4l2_to_ffmeg(pixformat);
    codec = avcodec_find_decoder(codec_id);
    if (!codec)
        return AVERROR_DECODER_NOT_FOUND;

    new_converter = g_new0(struct img_ffmpeg_data, 1);
    new_converter->codec = avcodec_alloc_context3(codec);
    new_converter->frame = av_frame_alloc();
    new_converter->packet = av_packet_alloc();

    if (!new_converter->codec || !new_converter->frame ||
	!new_converter->packet) {

        img_ffmpeg_free(new_converter);
        return AVERROR(ENOMEM);
    }

    ret = avcodec_open2(new_converter->codec, codec, NULL);
    if (ret < 0) {
        img_ffmpeg_free(new_converter);
        return ret;
    }

    new_converter->pixformat = pixformat;
    *converter = new_converter;

    return 0;
}

static int convert_decoded_frame(struct img_ffmpeg_data *state, uint8_t *rgb,
                                 size_t rgb_stride, unsigned char *output,
                                 unsigned int width, unsigned int height)
{
    int destination_stride[4] = { (int)rgb_stride, 0, 0, 0 };
    uint8_t *destination[4] = { rgb, NULL, NULL, NULL };
    size_t row_size = (size_t)width * 3;
    enum AVPixelFormat source_format;
    enum AVColorSpace color_space;
    const int *coefficients;
    int source_range;
    int ret;

    source_format = state->frame->format;
    color_space = state->frame->colorspace;
    switch (source_format) {
    case AV_PIX_FMT_YUVJ420P:
        source_format = AV_PIX_FMT_YUV420P;
        break;
    case AV_PIX_FMT_YUVJ422P:
        source_format = AV_PIX_FMT_YUV422P;
        break;
    case AV_PIX_FMT_YUVJ444P:
        source_format = AV_PIX_FMT_YUV444P;
        break;
    case AV_PIX_FMT_YUVJ440P:
        source_format = AV_PIX_FMT_YUV440P;
        break;
    default:
        break;
    }

    switch (color_space) {
    case AVCOL_SPC_BT709:
        coefficients = sws_getCoefficients(SWS_CS_ITU709);
        break;
    case AVCOL_SPC_BT470BG:
    case AVCOL_SPC_SMPTE170M:
        coefficients = sws_getCoefficients(SWS_CS_ITU601);
        break;
    case AVCOL_SPC_SMPTE240M:
        coefficients = sws_getCoefficients(SWS_CS_SMPTE240M);
        break;
    case AVCOL_SPC_BT2020_NCL:
    case AVCOL_SPC_BT2020_CL:
        coefficients = sws_getCoefficients(SWS_CS_BT2020);
        break;
    default:
        /* Match V4L2's SDTV/HDTV fallback when the stream has no tag. */
        coefficients = sws_getCoefficients(state->frame->height <= 576 ?
					   SWS_CS_ITU601 : SWS_CS_ITU709);
        break;
    }

    state->sws = sws_getCachedContext(state->sws,
                                      state->frame->width,
                                      state->frame->height,
                                      source_format,
                                      width, height, AV_PIX_FMT_RGB24,
                                      SWS_BILINEAR, NULL, NULL, NULL);
    if (!state->sws)
        return 0;

    source_range = state->frame->color_range == AVCOL_RANGE_JPEG ||
                   state->frame->format == AV_PIX_FMT_YUVJ420P ||
                   state->frame->format == AV_PIX_FMT_YUVJ422P ||
                   state->frame->format == AV_PIX_FMT_YUVJ444P ||
                   state->frame->format == AV_PIX_FMT_YUVJ440P;
    ret = sws_setColorspaceDetails(state->sws, coefficients, source_range,
                                   coefficients, 1, 0, 1 << 16, 1 << 16);
    if (ret < 0)
        return 0;

    ret = sws_scale(state->sws,
                    (const uint8_t *const *)state->frame->data,
                    state->frame->linesize, 0, state->frame->height,
                    destination, destination_stride);
    if (ret != (int)height)
        return 0;

    for (unsigned int y = 0; y < height; y++)
        memcpy(output + y * row_size, rgb + y * rgb_stride, row_size);

    return row_size * height;
}

static int drain_decoder(struct img_ffmpeg_data *state, uint8_t *rgb,
                         size_t rgb_stride, unsigned char *output,
                         unsigned int width, unsigned int height,
                         int *bytes_out)
{
    int received = 0;
    int ret;

    while (1) {
        ret = avcodec_receive_frame(state->codec, state->frame);
        if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF)
            return received;
        if (ret < 0)
            return ret;

        received++;
        ret = convert_decoded_frame(state, rgb, rgb_stride, output,
                                    width, height);
        if (ret > 0)
            *bytes_out = ret;

        av_frame_unref(state->frame);
    }
}

int img_ffmpeg_to_rgb24(struct img_ffmpeg_data **converter,
                        unsigned int pixformat, const unsigned char *input,
                        size_t input_size, unsigned char *output,
                        unsigned int width, unsigned int height)
{
    size_t row_size, rgb_stride, rgb_size;
    struct img_ffmpeg_data *state;
    uint8_t *rgb;
    int bytes_out = 0;
    int ret;

    if (!converter || !input || !output || !input_size || input_size > INT_MAX)
        return 0;

    row_size = (size_t)width * 3;
    if (!width || !height || row_size > INT_MAX - 63)
        return 0;

    rgb_stride = (row_size + 63) & ~(size_t)63;
    if (rgb_stride > SIZE_MAX / height)
        return 0;

    rgb_size = rgb_stride * height;
    rgb = av_mallocz(rgb_size + AV_INPUT_BUFFER_PADDING_SIZE);
    if (!rgb)
        return 0;

    ret = img_ffmpeg_prepare(converter, pixformat);
    if (ret < 0)
        goto out;

    state = *converter;
    ret = av_new_packet(state->packet, input_size);
    if (ret < 0)
        goto out;

    memcpy(state->packet->data, input, input_size);

    /* Drain delayed output and retry the same packet. Dropping an H.264
     * packet here would break reference-frame continuity. */
    while (1) {
        ret = avcodec_send_packet(state->codec, state->packet);
        if (ret != AVERROR(EAGAIN))
            break;
        ret = drain_decoder(state, rgb, rgb_stride, output, width, height,
                            &bytes_out);
        if (ret <= 0) {
            av_packet_unref(state->packet);
            goto out;
        }
    }

    av_packet_unref(state->packet);
    if (ret < 0)
        goto out;

    drain_decoder(state, rgb, rgb_stride, output, width, height, &bytes_out);

out:
    av_free(rgb);
    return bytes_out;
}
