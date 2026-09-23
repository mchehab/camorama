// SPDX-License-Identifier: GPL-2.0-or-later
#ifndef TESTS_TEST_UTILS_H
#define TESTS_TEST_UTILS_H

#include <stddef.h>
#include <stdint.h>
#include <sys/types.h>

char *test_get_executable_dir(void);
int test_run_program(char *const argv[]);
double test_estimate_psnr(const unsigned char *original,
                          const unsigned char *reconstructed,
                          size_t size);
int test_save_png(const char *path, const unsigned char *buffer,
                  unsigned int width, unsigned int height);
int test_save_png_named(const char *name, const unsigned char *buffer,
                        unsigned int width, unsigned int height,
                        int original);

#endif
