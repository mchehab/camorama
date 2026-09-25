// SPDX-License-Identifier: GPL-2.0-or-later

#define _GNU_SOURCE
#include <errno.h>
#include <stdbool.h>
#include <libgen.h>
#include <limits.h>
#include <math.h>
#include <png.h>
#include <setjmp.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "unittest.h"
#include "test_utils.h"

char *test_get_executable_dir(void)
{
    char exe[PATH_MAX];
    ssize_t size = readlink("/proc/self/exe", exe, sizeof(exe) - 1);
    char *copy;

    if (size <= 0)
        return NULL;
    exe[size] = '\0';
    copy = strdup(exe);
    if (!copy)
        return NULL;
    return dirname(copy);
}

int test_run_program(char *const argv[])
{
    pid_t pid = fork();
    int status;

    if (pid < 0)
        return -errno;
    if (pid == 0) {
        execvp(argv[0], argv);
        _exit(127);
    }
    while (waitpid(pid, &status, 0) < 0) {
        if (errno == EINTR)
            continue;
        return -errno;
    }
    if (!WIFEXITED(status))
        return -EINTR;
    return WEXITSTATUS(status);
}

const char *test_ffmpeg_h264_encoder(bool require_b_frames)
{
    /* Hardware encoders can be listed even when their device is unavailable.
     * Keep these fixture tests reproducible by selecting software encoders. */
    static const char *const software_preference[] = {
        "libx264", "libopenh264",
    };
    static const char *const b_frame_preference[] = {
        "libx264",
    };
    static char selected[2][64];
    static bool initialized[2];
    static bool available[2];
    unsigned int preference_index = require_b_frames;
    const char *const *preference = require_b_frames ? b_frame_preference :
                                                         software_preference;
    unsigned int preference_count = require_b_frames ?
        ARRAY_SIZE(b_frame_preference) : ARRAY_SIZE(software_preference);
    char encoders[32][64];
    unsigned int num_encoders = 0, i, j;
    int fds[2], status;
    pid_t waited;
    pid_t pid;
    FILE *output;
    char *line = NULL;
    size_t line_size = 0;

    if (initialized[preference_index])
        return available[preference_index] ? selected[preference_index] : NULL;
    initialized[preference_index] = true;

    if (pipe(fds) < 0)
        return NULL;
    pid = fork();
    if (pid < 0) {
        close(fds[0]);
        close(fds[1]);
        return NULL;
    }
    if (!pid) {
        close(fds[0]);
        if (dup2(fds[1], STDOUT_FILENO) < 0)
            _exit(127);
        close(fds[1]);
        execl(FFMPEG_BIN, FFMPEG_BIN, "-hide_banner", "-encoders", NULL);
        _exit(127);
    }

    close(fds[1]);
    output = fdopen(fds[0], "r");
    if (!output) {
        close(fds[0]);
        waitpid(pid, &status, 0);
        return NULL;
    }
    while (getline(&line, &line_size, output) >= 0) {
        char *saveptr = NULL;
        char *flags = strtok_r(line, " \t\r\n", &saveptr);
        char *name = strtok_r(NULL, " \t\r\n", &saveptr);

        if (!flags || flags[0] != 'V' || !name ||
            !strstr(name, "264") || !strcmp(name, "libx264rgb") ||
            num_encoders == ARRAY_SIZE(encoders))
            continue;
        snprintf(encoders[num_encoders], sizeof(encoders[num_encoders]),
                 "%s", name);
        num_encoders++;
    }
    free(line);
    fclose(output);
    do {
        waited = waitpid(pid, &status, 0);
    } while (waited < 0 && errno == EINTR);
    if (waited < 0 || !WIFEXITED(status) || WEXITSTATUS(status))
        return NULL;

    for (i = 0; i < preference_count; i++) {
        for (j = 0; j < num_encoders; j++) {
            if (strcmp(preference[i], encoders[j]))
                continue;
            snprintf(selected[preference_index],
                     sizeof(selected[preference_index]), "%s", encoders[j]);
            available[preference_index] = true;
            return selected[preference_index];
        }
    }
    return NULL;
}

double test_estimate_psnr(const unsigned char *original,
                          const unsigned char *reconstructed, size_t size)
{
    double mse = 0.0;
    size_t i;

    if (!size)
        return 0.0;
    for (i = 0; i < size; i++) {
        double delta = (double)original[i] - (double)reconstructed[i];
        mse += delta * delta;
    }
    mse /= size;
    if (mse == 0.0)
        return 999.0;
    return 10.0 * log10((255.0 * 255.0) / mse);
}

int test_save_png(const char *path, const unsigned char *buffer,
                  unsigned int width, unsigned int height)
{
    png_structp png = NULL;
    png_infop info = NULL;
    png_bytep *rows = NULL;
    FILE *fp = NULL;
    unsigned int y;
    int rc = -1;

    fp = fopen(path, "wb");
    if (!fp)
        goto done;
    png = png_create_write_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
    if (!png)
        goto done;
    info = png_create_info_struct(png);
    if (!info)
        goto done;
    if (setjmp(png_jmpbuf(png)))
        goto done;

    png_init_io(png, fp);
    png_set_IHDR(png, info, width, height, 8, PNG_COLOR_TYPE_RGB,
                 PNG_INTERLACE_NONE, PNG_COMPRESSION_TYPE_DEFAULT,
                 PNG_FILTER_TYPE_DEFAULT);
    png_write_info(png, info);
    rows = malloc(sizeof(*rows) * height);
    if (!rows)
        goto done;
    for (y = 0; y < height; y++)
        rows[y] = (png_bytep)(buffer + (size_t)y * width * 3);
    png_write_image(png, rows);
    png_write_end(png, NULL);
    rc = 0;

done:
    free(rows);
    if (png)
        png_destroy_write_struct(&png, info ? &info : NULL);
    if (fp)
        fclose(fp);
    return rc;
}

int test_save_png_named(const char *name, const unsigned char *buffer,
                        unsigned int width, unsigned int height,
                        int original)
{
    char *path = NULL;
    int rc = asprintf(&path, "/tmp/%s-%s.png", name,
                      original ? "original" : "reconstructed");
    if (rc < 0)
        return -1;
    rc = test_save_png(path, buffer, width, height);
    free(path);
    return rc;
}
