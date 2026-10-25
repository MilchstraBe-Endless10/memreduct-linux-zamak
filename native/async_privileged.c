#define _POSIX_C_SOURCE 200809L
#include <errno.h>
#include <stdlib.h>
#include <stdint.h>
#include <spawn.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

extern char **environ;
static pid_t child_pid = -1;

/* Return 0 on success, or a positive errno (or EBUSY). GUI stays unprivileged. */
int64_t mr_start_reclaim(int64_t mode) {
    if (child_pid > 0) return EBUSY;
    if (mode < 0 || mode > 2) return EINVAL;
    const char *action = mode == 0 ? "pagecache" : mode == 1 ? "metadata" : "all";
    /* Production uses an absolute path; only the test build may override it. */
    const char *binary = "/usr/bin/pkexec";
#ifdef MEMREDUCT_TESTING
    const char *override = getenv("MEMREDUCT_TEST_PKEXEC");
    if (override && *override == '/') binary = override;
#endif
    char *const args[] = {
        (char *)binary, "/usr/libexec/memreduct-zamak-helper", (char *)action, NULL
    };
    pid_t p = -1;
    int status = posix_spawn(&p, binary, NULL, NULL, args, environ);
    if (status != 0) return status;
    child_pid = p;
    return 0;
}

/* -2 idle, -1 running, 0 success, otherwise a nonzero exit status. */
int64_t mr_poll_reclaim(void) {
    if (child_pid < 0) return -2;
    int status = 0;
    pid_t result = waitpid(child_pid, &status, WNOHANG);
    if (result == 0) return -1;
    child_pid = -1;
    if (result < 0) return 125;
    if (WIFEXITED(status)) return WEXITSTATUS(status);
    if (WIFSIGNALED(status)) return 128 + WTERMSIG(status);
    return 126;
}
