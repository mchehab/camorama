// SPDX-License-Identifier: GPL-2.0-or-later
/*
* Copyright (C) 2026 Mauro Carvalho Chehab <mchehab+huawei@kernel.org>
*/

#include <errno.h>
#include <stdbool.h>
#include <stdlib.h>
#include <sys/queue.h>


struct module_test_runtime {
    int (*run)(void);
    unsigned int priority;

    LIST_ENTRY(module_test_runtime) node;
};

LIST_HEAD(module_test_list, module_test_runtime);

static struct module_test_list module_tests = LIST_HEAD_INITIALIZER(module_tests);

int module_test_register(int (*run)(void), unsigned int priority)
{
    struct module_test_runtime *test, *new, *prev = NULL;

    LIST_FOREACH(test, &module_tests, node) {
        if (test->run == run)
            return -EEXIST;
    }

    new = malloc(sizeof(*new));
    if (!new)
        return -ENOMEM;

    new->run = run;
    new->priority = priority;

    LIST_FOREACH(test, &module_tests, node) {
        if (priority < test->priority) {
            LIST_INSERT_BEFORE(test, new, node);
            return 0;
        }

        prev = test;
    }

    if (prev)
        LIST_INSERT_AFTER(prev, new, node);
    else
        LIST_INSERT_HEAD(&module_tests, new, node);

    return 0;
}


/*
* Instead of creating a header file for each test, let's just add them
* all here, in alphabetic order.
*/
int test_img_convert(void);

int main(void)
{
    struct module_test_runtime *test;
    int failed = 0;

    LIST_FOREACH(test, &module_tests, node) {
        if (test->run())
            failed++;
    }


    return failed != 0;
}
