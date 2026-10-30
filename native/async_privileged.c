#define _POSIX_C_SOURCE 200809L
#include <errno.h>
#include <stdlib.h>
#include <stdint.h>
#include <spawn.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include "async_privileged.h"

extern char **environ;
static pid_t child_pid = -1;

/* The GUI stays unprivileged; only the fixed pagecache enum reaches pkexec. */
int64_t mr_start_reclaim(int64_t mode) {
    if (mode != 0) return MR_RECLAIM_UNSUPPORTED_ACTION;
    if (child_pid > 0) return MR_RECLAIM_BUSY;
    const char *action = "pagecache";
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
    if (status != 0) return MR_RECLAIM_LAUNCH_FAILED;
    child_pid = p;
    return MR_RECLAIM_STARTED;
}

/* Normalize pkexec/helper results. Never retry: an uncertain write may have applied. */
int64_t mr_poll_reclaim(void) {
    if (child_pid < 0) return MR_RECLAIM_IDLE;
    int status = 0;
    pid_t result = waitpid(child_pid, &status, WNOHANG);
    if (result == 0 || (result < 0 && errno == EINTR)) return MR_RECLAIM_RUNNING;
    child_pid = -1;
    if (result < 0) return MR_RECLAIM_WAIT_FAILED;
    if (WIFSIGNALED(status) || !WIFEXITED(status)) return MR_RECLAIM_CHILD_ABNORMAL;

    switch (WEXITSTATUS(status)) {
        case 0: return MR_RECLAIM_ACCEPTED;
        /* pkexec(1): dismissed authentication returns 126. */
        case 126: return MR_RECLAIM_AUTH_CANCELLED;
        /* pkexec(1): authorization or command launch failure returns 127. */
        case 127: return MR_RECLAIM_AUTH_OR_PKEXEC_FAILED;
        case 2: return MR_RECLAIM_HELPER_UNSUPPORTED_ACTION;
        case 3: return MR_RECLAIM_OUTCOME_UNCERTAIN;
        default: return MR_RECLAIM_HELPER_FAILED;
    }
}
