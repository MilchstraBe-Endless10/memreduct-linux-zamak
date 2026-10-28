/* SPDX-License-Identifier: GPL-3.0-or-later */
#define _POSIX_C_SOURCE 200809L
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>

ssize_t __real_write(int fd, const void *buffer, size_t count);
int __real_close(int fd);

ssize_t __wrap_write(int fd, const void *buffer, size_t count) {
    static int write_fault_used = 0;
    const char *fault = getenv("MEMREDUCT_TEST_WRITE_FAULT");
    if (!fault || write_fault_used)
        return __real_write(fd, buffer, count);

    write_fault_used = 1;
    if (strcmp(fault, "eintr-once") == 0) {
        errno = EINTR;
        return -1;
    }
    if (strcmp(fault, "write-eio-once") == 0) {
        errno = EIO;
        return -1;
    }
    if (strcmp(fault, "short-once") == 0)
        return __real_write(fd, buffer, count > 0 ? 1 : 0);
    return __real_write(fd, buffer, count);
}

int __wrap_close(int fd) {
    static int close_fault_used = 0;
    const char *fault = getenv("MEMREDUCT_TEST_WRITE_FAULT");
    int result = __real_close(fd);
    if (fault && strcmp(fault, "close-fail-once") == 0 && !close_fault_used) {
        close_fault_used = 1;
        errno = EIO;
        return -1;
    }
    return result;
}
