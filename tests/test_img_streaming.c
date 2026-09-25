// SPDX-License-Identifier: GPL-2.0-or-later

#define _GNU_SOURCE
#include <errno.h>
#include <limits.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdarg.h>
#include <setjmp.h>
#include <cmocka.h>
#include <libavcodec/avcodec.h>
#include <libavutil/error.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "unittest.h"
#include "src/img_convert.h"
#include "src/img_ffmpeg.h"
#include "src/camera-backend.h"
#include "test_utils.h"

static const unsigned int MAX_FRAMES = 150;
static const unsigned int STREAM_FPS = 15;

struct streaming_case {
    const char *extension;
    unsigned int pixformat;
    enum AVCodecID codec_id;
};

static char *stream_path(const char *extension)
{
    char *dir = test_get_executable_dir();
    char *path = NULL;
    assert_non_null(dir);
    assert_true(asprintf(&path, "%s/camorama_stream.%s", dir, extension) > 0);
    free(dir);
    return path;
}

static void generate_stream(const struct streaming_case *test, char **path_out)
{
    char *path = stream_path(test->extension);
    char *argv[32];
    char frame_arg[16];
    char input_arg[128];
    int argc = 0, status;
    struct stat st;

    snprintf(frame_arg, sizeof(frame_arg), "%u", MAX_FRAMES);
    snprintf(input_arg, sizeof(input_arg),
             "smptebars=size=320x240:rate=%u:duration=%.3f,scroll=h=0.08,scroll=v=0.002",
             STREAM_FPS, (double)MAX_FRAMES / STREAM_FPS);
    argv[argc++] = (char *)FFMPEG_BIN;
    argv[argc++] = "-hide_banner";
    argv[argc++] = "-loglevel";
    argv[argc++] = "error";
    argv[argc++] = "-y";
    argv[argc++] = "-f";
    argv[argc++] = "lavfi";
    argv[argc++] = "-i";
    argv[argc++] = input_arg;
    argv[argc++] = "-frames:v";
    argv[argc++] = frame_arg;
    argv[argc++] = "-an";
    if (test->pixformat == V4L2_PIX_FMT_H264) {
        argv[argc++] = "-c:v";
        const char *encoder = test_ffmpeg_h264_encoder(true);

        assert_non_null(encoder);
        argv[argc++] = (char *)encoder;
        argv[argc++] = "-bf";
        argv[argc++] = "2";
        argv[argc++] = "-g";
        argv[argc++] = "30";
        argv[argc++] = "-f";
        argv[argc++] = "h264";
    } else {
        argv[argc++] = "-c:v";
        argv[argc++] = "mjpeg";
        argv[argc++] = "-q:v";
        argv[argc++] = "2";
        argv[argc++] = "-f";
        argv[argc++] = "mjpeg";
    }
    argv[argc++] = path;
    argv[argc] = NULL;

    status = test_run_program(argv);
    assert_int_equal(status, 0);
    assert_int_equal(stat(path, &st), 0);
    assert_true(st.st_size > 0);
    *path_out = path;
}

static unsigned int output_width(unsigned int frame)
{
    static const unsigned int widths[] = { 160, 240, 320 };
    return widths[frame % 3];
}

static unsigned int output_height(unsigned int frame)
{
    static const unsigned int heights[] = { 120, 180, 240 };
    return heights[frame % 3];
}

static void stream_test(void **state)
{
    const struct streaming_case *test = *state;
    char *path = NULL;
    struct stat st;
    unsigned char *data;
    FILE *fp;
    size_t size, got;
    unsigned int frames = 0;
    unsigned char *first_frames[3] = { NULL };
    bool changed[3] = { false };
    bool seen[3] = { false };
    cam_t cam = { 0 };
    unsigned int i;

    generate_stream(test, &path);

    printf("Video stored at %s\n", path);

    cam.pixformat = test->pixformat;
    assert_int_equal(stat(path, &st), 0);
    fp = fopen(path, "rb");
    assert_non_null(fp);
    size = st.st_size;
    data = malloc(size);
    assert_non_null(data);
    got = fread(data, 1, size, fp);
    fclose(fp);
    assert_int_equal(got, size);

    if (test->pixformat == V4L2_PIX_FMT_MJPEG) {
        size_t start = 0;
        for (i = 0; i + 1 < size && frames < MAX_FRAMES; i++) {
            if (data[i] == 0xff && data[i + 1] == 0xd8) {
                start = i;
                for (i += 2; i + 1 < size; i++) {
                    if (data[i] == 0xff && data[i + 1] == 0xd9) {
                        unsigned int bucket = frames % 3;
                        unsigned int width = output_width(frames);
                        unsigned int height = output_height(frames);
                        unsigned char *output = malloc((size_t)width * height * 3);
                        assert_non_null(output);
                        cam.width = width;
                        cam.height = height;
                        cam.sizeimage = i + 2 - start;
                        assert_int_equal(img_convert_to_rgb24(&cam,
                            (unsigned char *)data + start, cam.sizeimage, output),
                            width * height * 3);
                        if (seen[bucket])
                            changed[bucket] |= test_estimate_psnr(first_frames[bucket],
                                output, (size_t)width * height * 3) < 60.0;
                        else {
                            first_frames[bucket] = malloc((size_t)width * height * 3);
                            assert_non_null(first_frames[bucket]);
                            memcpy(first_frames[bucket], output,
                                   (size_t)width * height * 3);
                        }
                        seen[bucket] = true;
                        free(output);
                        frames++;
                        i++;
                        break;
                    }
                }
            }
        }
    } else {
        const AVCodec *codec = avcodec_find_decoder(test->codec_id);
        AVCodecContext *context;
        AVCodecParserContext *parser;
        size_t offset = 0;
        assert_non_null(codec);
        context = avcodec_alloc_context3(codec);
        parser = av_parser_init(test->codec_id);
        assert_non_null(context);
        assert_non_null(parser);
        while (offset < size && frames < MAX_FRAMES) {
            uint8_t *packet = NULL;
            int packet_size = 0;
            int consumed = av_parser_parse2(parser, context, &packet,
                                            &packet_size, data + offset,
                                            size - offset, AV_NOPTS_VALUE,
                                            AV_NOPTS_VALUE, 0);
            assert_true(consumed >= 0);
            offset += consumed;
            if (packet_size)
                {
                    unsigned int bucket = frames % 3;
                    unsigned int width = output_width(frames);
                    unsigned int height = output_height(frames);
                    size_t output_size = (size_t)width * height * 3;
                    unsigned char *output = malloc(output_size);
                    unsigned int converted;
                    assert_non_null(output);
                    cam.width = width;
                    cam.height = height;
                    cam.sizeimage = packet_size;
                    converted = img_convert_to_rgb24(&cam, packet,
                                                     cam.sizeimage, output);
                    if (!converted) {
                        free(output);
                        continue;
                    }
                    assert_int_equal(converted, output_size);
                    if (seen[bucket])
                        changed[bucket] |= test_estimate_psnr(first_frames[bucket],
                            output, output_size) < 60.0;
                    else {
                        first_frames[bucket] = malloc(output_size);
                        assert_non_null(first_frames[bucket]);
                        memcpy(first_frames[bucket], output, output_size);
                    }
                    seen[bucket] = true;
                    free(output);
                    frames++;
                }
            if (!consumed && !packet_size)
                break;
        }
        while (frames < MAX_FRAMES) {
            uint8_t *packet = NULL;
            int packet_size = 0;
            int consumed = av_parser_parse2(parser, context, &packet,
                                            &packet_size, NULL, 0,
                                            AV_NOPTS_VALUE, AV_NOPTS_VALUE, 0);
            assert_true(consumed >= 0);
            if (!packet_size)
                break;
            {
                unsigned int bucket = frames % 3;
                unsigned int width = output_width(frames);
                unsigned int height = output_height(frames);
                size_t output_size = (size_t)width * height * 3;
                unsigned char *output = malloc(output_size);
                unsigned int converted;
                assert_non_null(output);
                cam.width = width;
                cam.height = height;
                cam.sizeimage = packet_size;
                converted = img_convert_to_rgb24(&cam, packet,
                                                 cam.sizeimage, output);
                if (!converted) {
                    free(output);
                    continue;
                }
                assert_int_equal(converted, output_size);
                if (seen[bucket])
                    changed[bucket] |= test_estimate_psnr(first_frames[bucket],
                        output, output_size) < 60.0;
                else {
                    first_frames[bucket] = malloc(output_size);
                    assert_non_null(first_frames[bucket]);
                    memcpy(first_frames[bucket], output, output_size);
                }
                seen[bucket] = true;
                free(output);
                frames++;
            }
        }
        av_parser_close(parser);
        avcodec_free_context(&context);
    }

    if (test->pixformat == V4L2_PIX_FMT_H264)
        assert_true(frames >= MAX_FRAMES - 2);
    else
        assert_int_equal(frames, MAX_FRAMES);
    for (i = 0; i < 3; i++)
        assert_true(seen[i]);
    for (i = 0; i < 3; i++)
        assert_true(changed[i]);

    img_ffmpeg_free_converter(&cam.converter);
    for (i = 0; i < 3; i++)
        free(first_frames[i]);
    free(data);
    free(path);
}

static void test_mjpeg_stream(void **state)
{
    (void)state;
    static const struct streaming_case test = {
        "mjpg", V4L2_PIX_FMT_MJPEG, AV_CODEC_ID_MJPEG,
    };
    void *test_state = (void *)&test;
    stream_test(&test_state);
}

static void test_h264_stream(void **state)
{
    (void)state;
    if (!test_ffmpeg_h264_encoder(true)) {
        fprintf(stderr, "Skipping H.264 stream: FFmpeg has no supported software H.264 encoder with B-frame support\n");
        skip();
    }
    static const struct streaming_case test = {
        "h264", V4L2_PIX_FMT_H264, AV_CODEC_ID_H264,
    };
    void *test_state = (void *)&test;
    stream_test(&test_state);
}

static int test_img_streaming(void)
{
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(test_mjpeg_stream),
        cmocka_unit_test(test_h264_stream),
    };
    return _cmocka_run_group_tests("streaming", tests,
                                   sizeof(tests) / sizeof(tests[0]), NULL, NULL);
}

REGISTER_TEST(test_img_streaming, 0);
