/* SPDX-License-Identifier: GPL-3.0-or-later */
#define _POSIX_C_SOURCE 200809L
#include <errno.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/wait.h>

pid_t __real_waitpid(pid_t pid, int *status, int options);

pid_t __wrap_waitpid(pid_t pid, int *status, int options) {
    static int injected = 0;
    const char *fault = getenv("MEMREDUCT_TEST_WAITPID_EINTR_ONCE");
    if (fault != NULL && fault[0] == '1' && !injected) {
        injected = 1;
        errno = EINTR;
        return -1;
    }
    return __real_waitpid(pid, status, options);
}
