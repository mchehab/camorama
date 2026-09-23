// SPDX-License-Identifier: GPL-2.0-or-later

#define _GNU_SOURCE
#include <errno.h>
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
