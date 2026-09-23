// SPDX-License-Identifier: GPL-2.0-or-later
#ifndef TESTS_MOCK_V4L_H
#define TESTS_MOCK_V4L_H

#include "src/v4l.h"

struct mock_v4l_size {
    unsigned int width, height;
    float max_fps;
};

extern const struct mock_v4l_size mock_c920_yuyv_sizes[];
extern const unsigned int mock_c920_yuyv_sizes_count;
extern const struct mock_v4l_size mock_c920_compressed_sizes[];
extern const unsigned int mock_c920_compressed_sizes_count;
extern const struct cam_v4l_ops mock_c920_v4l_ops;

#endif
