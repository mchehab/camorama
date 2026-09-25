// SPDX-License-Identifier: GPL-2.0-or-later
/*
* Copyright (C) 2026 Mauro Carvalho Chehab <mchehab+huawei@kernel.org>
*/

#include <argp.h>
#include <errno.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <sys/queue.h>
#include <unistd.h>

#include "unittest.h"

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
 * Add support for some arguments to control output format
 */
const char *argp_program_version = "camorama unit tests 0.1.0";
const char *argp_program_bug_address = "mchehab@kernel.org";

struct arguments {
	const char *selected_group;
	const char *test_filter;
	const char *skip_filter;
	uint32_t output_formats;
	bool output_was_set;
	bool no_mock;
};

static const struct argp_option options[] = {
	{ "output", 'o', "FORMAT", 0, "Output format; may be repeated: standard, tap, xml, subunit", 0 },
	{ "filter", 'f', "PATTERN", 0, "Run test names matching PATTERN; supports '*' and '?'", 0 },
	{ "skip", 's', "PATTERN", 0, "Skip test names matching PATTERN; supports '*' and '?'", 0 },
	{ "group", 'g', "NAME", 0, "Run only the named test group", 0 },
	{ "no-mock", 'n', NULL, 0, "Don't mock logs", 0 },
	{ "list-groups", 'l', NULL, 0, "List available test groups", 0 },
	{ 0 }
};

static const char doc[] = "Run camorama unit tests.";


static uint32_t parse_output_format(const char *value,
                                    struct argp_state *state)
{
	if (strcasecmp(value, "standard") == 0)
		return CM_OUTPUT_STDOUT;

	if (strcasecmp(value, "tap") == 0)
		return CM_OUTPUT_TAP;

	if (strcasecmp(value, "xml") == 0)
		return CM_OUTPUT_XML;

	if (strcasecmp(value, "subunit") == 0)
		return CM_OUTPUT_SUBUNIT;

	argp_error(state,
		   "unknown format: %s. Expected: standard, tap, xml, or subunit",
		   value);

	return 0;
}

static error_t parse_option(int key, char *value, struct argp_state *state)
{
	struct arguments *args = state->input;

	switch (key) {
	case 'o':
		args->output_formats |= parse_output_format(value, state);
		args->output_was_set = true;
		return 0;

	case 'f':
			args->test_filter = value;
			return 0;

	case 's':
			args->skip_filter = value;
			return 0;

	case 'g':
			args->selected_group = value;
			return 0;

	case 'n':
			args->no_mock = true;
			return 0;

	case ARGP_KEY_ARG:
			argp_error(state,
				   "unexpected positional value '%s'", value);
			return 0;

	case ARGP_KEY_END:
			return 0;

	default:
			return ARGP_ERR_UNKNOWN;
	}
}

static const struct argp argp = {
    .options = options,
    .parser = parse_option,
    .doc = doc,
};

/*
 * Some improvements for cmocka output format
 */
static bool stdout_is_vt = false;

#ifdef CMOCKA_VERSION_2
enum ansi_color {
    GREEN,
    RED,
    YELLOW,
    RESET,

    ANSI_MAX_COLORS
};

static const char *const codes[] = {
    [GREEN]  = "\033[32m",
    [RED]    = "\033[31;1m",
    [YELLOW] = "\033[33;1m",
    [RESET]  = "\033[0m"
};

static char running_test[256];
static bool running_line_open;

static const char *get_color(enum ansi_color color)
{
    if (!stdout_is_vt || color > ANSI_MAX_COLORS)
        return NULL;

    return codes[color];
}

static void save_test_name(const char *message)
{
    const char *name = &message[12];
    size_t len;

    while (*name == ' ')
        name++;

    len = strcspn(name, "\r\n");
    if (len >= sizeof(running_test))
        len = sizeof(running_test) - 1;

    memcpy(running_test, name, len);
    running_test[len] = '\0';
}

static bool is_running_test(const char *message)
{
    const char *name = &message[12];
    size_t len;

    while (*name == ' ')
        name++;

    len = strcspn(name, "\r\n");
    return strlen(running_test) == len &&
        !strncmp(running_test, name, len);
}

static bool is_test_result(const char *message)
{
    return !strncmp(message, "[       OK ]", 12) ||
        !strncmp(message, "[  FAILED  ]", 12) ||
        !strncmp(message, "[  SKIPPED ]", 12);
}

static void filter_output(const char *format, va_list args)
{
    char *message;
    const char *color = NULL;
    size_t len;
    bool keep_line_open = false;
    int rc;

    rc = vasprintf(&message, format, args);
    if (rc < 0)
        return;

    /*
    * NOTE: This is a poor man approach, as it assumes that formats
    *	 will use the first 12 chars for the type. A better way
    *	 would be to use XML or TAP and parse it, but this is simple
    *	 enough. Worse case scenario is that, if this changes, the log
    *	 won't use colors, which is not the end of times.
    */

    if (running_line_open) {
        if (is_test_result(message) && is_running_test(message))
            fputs("\r\033[2K", stdout);
        else
            fputc('\n', stdout);
        running_line_open = false;
        running_test[0] = '\0';
    }

    if (!(*message == '[') || strlen(message) < 12 || message[11] != ']') {
        fputs(message, stdout);
        free(message);
        return;
    }

    if (!strncmp(message, "[       OK ]", 12) ||
        !strncmp(message, "[  PASSED  ]", 12))
        color = get_color(GREEN);
    else if (!strncmp(message, "[  FAILED  ]", 12) ||
        !strncmp(message, "[   LINE   ]", 12) ||
        !strncmp(message, "[  ERROR   ]", 12))
        color = get_color(RED);
    else if (!strncmp(message, "[  SKIPPED ]", 12))
        color = get_color(YELLOW);

    if (stdout_is_vt && !strncmp(message, "[ RUN      ]", 12)) {
        save_test_name(message);
        running_line_open = true;
        keep_line_open = true;
    }

    fputc('[', stdout);

    if (color)
        fputs(color, stdout);

    for (int i = 1; i < 11; i++)
        fputc(message[i], stdout);

    color = get_color(RESET);
    if (color)
        fputs(color, stdout);

    fputc(']', stdout);

    len = strlen(&message[12]);
    if (keep_line_open)
        len = strcspn(&message[12], "\r\n");
    fwrite(&message[12], 1, len, stdout);
    if (keep_line_open)
        fflush(stdout);
    free(message);
}

const struct CMCallbacks callbacks = {
    .vprint_message = filter_output,
    .vprint_error = filter_output,
};
#endif

/*
* Instead of creating a header file for each test, let's just add them
* all here, in alphabetic order.
*/

int main(int argc, char **argv)
{
	struct arguments arguments = { 0 };
    struct module_test_runtime *test;
    int failed = 0;

    argp_parse(&argp, argc, argv, 0, NULL, &arguments);

    if (arguments.output_was_set)
        cmocka_set_message_output(arguments.output_formats);

    if (arguments.test_filter)
        cmocka_set_test_filter(arguments.test_filter);

    if (arguments.skip_filter)
        cmocka_set_skip_filter(arguments.skip_filter);

    #ifdef CMOCKA_VERSION_2
    cmocka_set_callbacks(&callbacks);
    #endif
    stdout_is_vt = isatty(fileno(stdout));

    LIST_FOREACH(test, &module_tests, node) {
        if (test->run())
            failed++;
    }


    return failed != 0;
}
