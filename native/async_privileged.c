#define _POSIX_C_SOURCE 200809L
#include <errno.h>
#include <stdlib.h>
#include <stdint.h>
#include <spawn.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include "async_privileged.h"

extern char **environ;
static pid_t child_pid = -1;
/* What the child process reported about itself, independent of its exit code.
 * pkexec forwards the child's exit status, so a numeric code alone cannot say
 * whether pkexec refused the request or the helper ran and failed that way. */
static int child_reported_helper_failure;

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
    child_reported_helper_failure = 0;
    child_pid = p;
    return MR_RECLAIM_STARTED;
}

/* mr_production_clean_enabled() is deliberately NOT defined here: it lives in
 * exactly one gate translation unit (production_clean_gate_default.c or
 * production_clean_gate_enabled.c) that the build selects. Two definitions
 * would be a link error, so a binary either has the bridge reachable or it
 * does not, and which one it is can be checked with nm. */

/* Explicit CI simulation only. This path executes a same-UID test launcher;
 * it never invokes pkexec or the installed helper. The launcher receives the
 * same fixed helper path and enum as the production bridge for contract tests. */
int64_t mr_start_reclaim_mock(int64_t mode) {
    const char *ci = getenv("MEMREDUCT_CI");
    const char *simulation = getenv("MEMREDUCT_CI_CLEAN_SIMULATION");
    const char *binary = getenv("MEMREDUCT_CI_MOCK_AUTHORIZER");
    struct stat st;
    if (mode != 0) return MR_RECLAIM_UNSUPPORTED_ACTION;
    if (child_pid > 0) return MR_RECLAIM_BUSY;
    if (ci == NULL || strcmp(ci, "1") != 0 || simulation == NULL || strcmp(simulation, "1") != 0 ||
        binary == NULL || binary[0] != '/' || lstat(binary, &st) != 0 || !S_ISREG(st.st_mode) ||
        st.st_uid != geteuid() || st.st_nlink != 1 || (st.st_mode & 0077) != 0 ||
        (st.st_mode & 0111) == 0) {
        return MR_RECLAIM_LAUNCH_FAILED;
    }
    char *const args[] = {
        (char *)binary, "/usr/libexec/memreduct-zamak-helper", "pagecache", NULL
    };
    pid_t p = -1;
    int status = posix_spawn(&p, binary, NULL, NULL, args, environ);
    if (status != 0) return MR_RECLAIM_LAUNCH_FAILED;
    child_reported_helper_failure = 0;
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

    int code = WEXITSTATUS(status);

    /* pkexec(1) forwards the child's exit status. 126 and 127 are documented as
     * pkexec's own "dismissed" and "authorization/launch failure" codes, but a
     * child that exits with the same number produces an identical status. The
     * only way to tell them apart is evidence the child produced about itself,
     * so an unreported 126/127 is reported as unattributed rather than guessed.
     * Parsing pkexec's stderr is not used: that text is not trustworthy input. */
    if (code == 126 || code == 127) {
        if (child_reported_helper_failure) return MR_RECLAIM_HELPER_FAILED;
        return MR_RECLAIM_OUTCOME_UNCERTAIN;
    }

    switch (code) {
        case 0: return MR_RECLAIM_ACCEPTED;
        case 2: return MR_RECLAIM_HELPER_UNSUPPORTED_ACTION;
        case 3: return MR_RECLAIM_OUTCOME_UNCERTAIN;
        default: return MR_RECLAIM_HELPER_FAILED;
    }
}

/* Called by the parent when the running child has told the UI which stage it
 * reached. The privileged bridge keeps no stage channel of its own, so this
 * exists for the phases the helper reports before it exits; anything the helper
 * does not report stays unattributed. */
int64_t mr_reclaim_child_reported_failure(int64_t reported) {
    if (child_pid < 0) return 0;
    child_reported_helper_failure = reported != 0 ? 1 : 0;
    return 1;
}
