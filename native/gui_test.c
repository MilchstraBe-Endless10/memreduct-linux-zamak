/* SPDX-License-Identifier: GPL-3.0-or-later */
#include <errno.h>
#include <stdlib.h>
#include <string.h>

/* Test controls are inert unless the CI runner sets these exact variables. */
long long mr_gui_test_mode(void)
{
    const char *value = getenv("MEMREDUCT_CI");
    return value != NULL && strcmp(value, "1") == 0 ? 1 : 0;
}

long long mr_gui_test_timeout_ms(void)
{
    const char *value = getenv("MEMREDUCT_CI_EXIT_MS");
    char *end = NULL;
    long parsed;

    if (value == NULL || value[0] == '\0') {
        return 0;
    }

    errno = 0;
    parsed = strtol(value, &end, 10);
    if (errno != 0 || end == value || *end != '\0' || parsed < 1000 || parsed > 120000) {
        return 0;
    }
    return (long long)parsed;
}
