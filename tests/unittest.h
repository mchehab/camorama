// SPDX-License-Identifier: GPL-2.0-or-later
/*
* Copyright (C) 2026 Mauro Carvalho Chehab <mchehab+huawei@kernel.org>
*/

#define _GNU_SOURCE
#include <stdio.h>
#include <setjmp.h>
#include <cmocka.h>

#include "img_convert.h"

#ifndef CMOCKA_VERSION_2
#define assert_non_null_msg(x, y) assert_non_null(x)
#define assert_null_msg(x, y) assert_null(x)
#endif

struct test_case {
    int (*fn)(void);
    const char *name;
};

int module_test_register(int (*run)(void),
                         unsigned int priority);

#define REGISTER_TEST(function, priority)                        \
    static void __attribute__((constructor)) register_##function(void)  \
    {                                                                   \
        module_test_register(function, priority);                       \
    }
