#define _POSIX_C_SOURCE 200809L
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

/* ONLY supported operations. Never execute a shell, accept paths or kill processes. */
int main(int argc, char **argv) {
    if (argc != 2) {
        fputs("usage: memreduct-zamak-helper {pagecache|metadata|all|--dry-run}\n", stderr);
        return 2;
    }
    if (strcmp(argv[1], "--dry-run") == 0) {
        puts("dry run OK; no kernel state modified");
        return 0;
    }
    const char *value = NULL;
    if (strcmp(argv[1], "pagecache") == 0) value = "1\n";
    if (strcmp(argv[1], "metadata") == 0) value = "2\n";
    if (strcmp(argv[1], "all") == 0) value = "3\n";
    if (!value) {
        fputs("unsupported cleanup action\n", stderr);
        return 2;
    }
    if (geteuid() != 0) {
        fputs("root privileges required; use pkexec\n", stderr);
        return 1;
    }
    int fd = open("/proc/sys/vm/drop_caches", O_WRONLY | O_CLOEXEC | O_NOFOLLOW);
    if (fd == -1) {
        fprintf(stderr, "cannot open drop_caches: %s\n", strerror(errno));
        return 1;
    }
    ssize_t written = write(fd, value, 2);
    int saved_errno = errno;
    (void)close(fd);
    if (written != 2) {
        fprintf(stderr, "cache reclaim failed: %s\n", strerror(saved_errno));
        return 1;
    }
    puts("kernel cache reclaim request accepted");
    return 0;
}
