#define _POSIX_C_SOURCE 200809L
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

/* Production actions use fixed values and a fixed path; never execute a shell or kill processes. */
int main(int argc, char **argv) {
    if (argc != 2) {
        fputs("usage: memreduct-zamak-helper {pagecache|--dry-run}\n", stderr);
        return 2;
    }
    if (strcmp(argv[1], "--dry-run") == 0) {
        puts("dry run OK; no kernel state modified");
        return 0;
    }
    if (strcmp(argv[1], "pagecache") != 0) {
        fputs("unsupported cleanup action\n", stderr);
        return 2;
    }
    const char value[] = "1\n";
#ifndef MEMREDUCT_TESTING
    if (geteuid() != 0) {
        fputs("root privileges required; use pkexec\n", stderr);
        return 1;
    }
#endif

    const char *path = "/proc/sys/vm/drop_caches";
#ifdef MEMREDUCT_TESTING
    path = getenv("MEMREDUCT_TEST_DROP_CACHES_PATH");
    if (!path || path[0] != '/') {
        fputs("test target must be an absolute path\n", stderr);
        return 2;
    }
#endif

    int fd = open(path, O_WRONLY | O_CLOEXEC | O_NOFOLLOW | O_NONBLOCK);
    if (fd == -1) {
        fprintf(stderr, "cannot open cleanup target: %s\n", strerror(errno));
        return 1;
    }

    struct stat info;
    if (fstat(fd, &info) == -1) {
        int saved_errno = errno;
        (void)close(fd);
        fprintf(stderr, "cannot inspect cleanup target: %s\n", strerror(saved_errno));
        return 1;
    }
    if (!S_ISREG(info.st_mode)) {
        (void)close(fd);
        fputs("cleanup target is not a regular file\n", stderr);
        return 1;
    }

    ssize_t written = write(fd, value, 2);
    if (written == -1 && errno == EINTR)
        written = write(fd, value, 2);

    int write_errno = written == -1 ? errno : 0;
    int close_result = close(fd);
    if (written == -1) {
        fprintf(stderr, "kernel rejected the cache reclaim request: %s\n", strerror(write_errno));
        return 1;
    }
    if (written != 2 || close_result == -1) {
        fprintf(stderr,
                "cache reclaim outcome is uncertain (write=%zd/2, close=%s); do not retry automatically\n",
                written, close_result == -1 ? "failed" : "ok");
        return 3;
    }
    puts("kernel accepted the cache reclaim request; reclaimed bytes are not measured");
    return 0;
}
