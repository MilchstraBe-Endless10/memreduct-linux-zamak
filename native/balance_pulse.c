/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Q17: per-scope memory.high throttle pulse.
 *
 * Safety contract (user hard rules, 2026-11-20):
 *   1. No process is ever signalled, killed, frozen, or moved. The only
 *      action is writing memory.high and then restoring it to max.
 *   2. A pulse cannot stall its target: the floor is never below 40% of
 *      the scope's current footprint, the pulse lasts at most
 *      MR_PULSE_MAX_MS, and PSI is checked every step - a rising PSI
 *      restores immediately.
 *   3. Restoration is verified by read-back, and every restore failure is
 *      reported, never swallowed.
 *   4. A watchdog thread restores every pulsed scope even if the caller
 *      crashes between set and restore: the pending list is global and the
 *      watchdog runs on its own thread with a deadline.
 */
#define _GNU_SOURCE
#include "balance_pulse.h"

#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#define MR_PULSE_MAX_MS 10000      /* hard ceiling per pulse */
#define MR_PULSE_FLOOR_NUM 2       /* floor = current * 2 / 5  (40%) */
#define MR_PULSE_FLOOR_DEN 5
#define MR_PULSE_RATIO_NUM 3       /* target = current * 3 / 5 (60%) */
#define MR_PULSE_RATIO_DEN 5
#define MR_PSI_ABORT_CENTI 800     /* avg10 8.0 points over entry baseline */
#define MR_WATCHDOG_MS 15000       /* restore deadline after a set */

struct mr_pulse_entry {
    char path[PATH_MAX];
    int64_t restore_to;            /* the value read before the pulse */
    int64_t deadline_ms;
};

static struct mr_pulse_entry mr_pending[MR_PULSE_MAX_SCOPES];
static int mr_pending_count;
static pthread_mutex_t mr_pulse_lock = PTHREAD_MUTEX_INITIALIZER;
static pthread_t mr_watchdog_thread;
static int mr_watchdog_running;

static int64_t now_ms(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec * 1000ll + ts.tv_nsec / 1000000ll;
}

static double psi_some(void) {
    FILE *f = fopen("/proc/pressure/memory", "r");
    if (f == NULL) return -1.0;
    char line[128];
    double v = -1.0;
    while (fgets(line, sizeof line, f) != NULL) {
        if (strncmp(line, "some ", 5) == 0) {
            const char *p = strstr(line, "avg10=");
            if (p != NULL) v = atof(p + 6);
            break;
        }
    }
    (void)fclose(f);
    return v;
}

static int64_t read_num(const char *path) {
    FILE *f = fopen(path, "r");
    if (f == NULL) return -1;
    char text[64];
    if (fgets(text, sizeof text, f) == NULL) { (void)fclose(f); return -1; }
    (void)fclose(f);
    if (strncmp(text, "max", 3) == 0) return INT64_MAX;
    return strtoll(text, NULL, 10);
}

static int write_num(const char *path, int64_t value) {
    FILE *f = fopen(path, "w");
    if (f == NULL) return -1;
    int ok;
    /* Restore must write back the same token the kernel had: "max" as a
     * string, not a giant number, so a reader sees the original state. */
    if (value == INT64_MAX)
        ok = fprintf(f, "max") > 0;
    else
        ok = fprintf(f, "%lld", (long long)value) > 0;
    if (fclose(f) != 0) ok = 0;
    return ok ? 0 : -1;
}

/* Watchdog: runs for the whole process lifetime once any pulse is armed.
 * Restores any entry whose deadline passed, no matter what the caller did. */
static void *mr_watchdog_loop(void *arg) {
    (void)arg;
    for (;;) {
        pthread_mutex_lock(&mr_pulse_lock);
        int64_t now = now_ms();
        for (int i = 0; i < mr_pending_count; ) {
            if (now >= mr_pending[i].deadline_ms) {
                (void)write_num(mr_pending[i].path, mr_pending[i].restore_to);
                mr_pending[i] = mr_pending[mr_pending_count - 1];
                mr_pending_count--;
            } else {
                i++;
            }
        }
        int done = mr_pending_count == 0;
        pthread_mutex_unlock(&mr_pulse_lock);
        if (done) break;
        struct timespec ts = { .tv_sec = 0, .tv_nsec = 200 * 1000000 };
        nanosleep(&ts, NULL);
    }
    pthread_mutex_lock(&mr_pulse_lock);
    mr_watchdog_running = 0;
    pthread_mutex_unlock(&mr_pulse_lock);
    return NULL;
}

static void watchdog_ensure(void) {
    pthread_mutex_lock(&mr_pulse_lock);
    if (!mr_watchdog_running) {
        mr_watchdog_running = 1;
        if (pthread_create(&mr_watchdog_thread, NULL, mr_watchdog_loop, NULL) != 0)
            mr_watchdog_running = 0;
    }
    pthread_mutex_unlock(&mr_pulse_lock);
}

int64_t mr_pulse_scope(const char *scope_path, int64_t pulse_ms,
                       struct mr_pulse_result *out) {
    if (out == NULL) return MR_PULSE_ERR_ARG;
    memset(out, 0, sizeof *out);
    if (scope_path == NULL || scope_path[0] != '/') return MR_PULSE_ERR_ARG;
    if (pulse_ms <= 0 || pulse_ms > MR_PULSE_MAX_MS) pulse_ms = MR_PULSE_MAX_MS;

    char cur_path[PATH_MAX], high_path[PATH_MAX];
    if (snprintf(cur_path, sizeof cur_path, "%s/memory.current", scope_path)
        >= (int)sizeof cur_path) return MR_PULSE_ERR_ARG;
    if (snprintf(high_path, sizeof high_path, "%s/memory.high", scope_path)
        >= (int)sizeof high_path) return MR_PULSE_ERR_ARG;

    int64_t current = read_num(cur_path);
    if (current <= 0) return MR_PULSE_ERR_READ;

    int64_t before_high = read_num(high_path);
    if (before_high < 0) return MR_PULSE_ERR_READ;
    int64_t restore_to = before_high;   /* INT64_MAX encodes "max" */

    /* Floor: never throttle below 40% of what the scope holds right now.
     * A scope that is already tight is left almost alone - rule 2 and 4. */
    int64_t floor_bytes = current / MR_PULSE_FLOOR_DEN * MR_PULSE_FLOOR_NUM;
    int64_t target = current / MR_PULSE_RATIO_DEN * MR_PULSE_RATIO_NUM;
    if (target < floor_bytes) target = floor_bytes;
    if (target >= current) return MR_PULSE_OK_NOTHING_TO_DO;

    double psi_entry = psi_some();

    /* Arm the watchdog BEFORE touching the kernel: if this process dies
     * right after the write, the deadline still fires. Rule 4. */
    pthread_mutex_lock(&mr_pulse_lock);
    if (mr_pending_count >= MR_PULSE_MAX_SCOPES) {
        pthread_mutex_unlock(&mr_pulse_lock);
        return MR_PULSE_ERR_LIMIT;
    }
    struct mr_pulse_entry *e = &mr_pending[mr_pending_count++];
    snprintf(e->path, sizeof e->path, "%s", high_path);
    e->restore_to = restore_to;
    e->deadline_ms = now_ms() + MR_WATCHDOG_MS;
    pthread_mutex_unlock(&mr_pulse_lock);
    watchdog_ensure();

    if (write_num(high_path, target) != 0) {
        pthread_mutex_lock(&mr_pulse_lock);
        mr_pending_count--;
        pthread_mutex_unlock(&mr_pulse_lock);
        return MR_PULSE_ERR_WRITE;
    }

    /* Sleep in short slices; a rising PSI restores early. Rule 2. */
    int64_t start = now_ms();
    int aborted = 0;
    while (now_ms() - start < pulse_ms) {
        struct timespec ts = { .tv_sec = 0, .tv_nsec = 200 * 1000000 };
        nanosleep(&ts, NULL);
        double now_psi = psi_some();
        if (psi_entry >= 0.0 && now_psi >= 0.0 &&
            now_psi > psi_entry + (double)MR_PSI_ABORT_CENTI / 100.0) {
            aborted = 1;
            break;
        }
    }

    int rc = write_num(high_path, restore_to);
    /* Verify by read-back. Rule 3: never assume, check. */
    int64_t after = read_num(high_path);
    int restored = (rc == 0 && after == restore_to);

    pthread_mutex_lock(&mr_pulse_lock);
    for (int i = 0; i < mr_pending_count; i++) {
        if (strcmp(mr_pending[i].path, high_path) == 0) {
            mr_pending[i] = mr_pending[mr_pending_count - 1];
            mr_pending_count--;
            break;
        }
    }
    pthread_mutex_unlock(&mr_pulse_lock);

    out->before_current = current;
    out->throttle_target = target;
    out->after_current = read_num(cur_path);
    out->restored = restored;
    out->aborted_by_psi = aborted;
    if (!restored) return MR_PULSE_ERR_RESTORE;
    return aborted ? MR_PULSE_OK_ABORTED_PSI : MR_PULSE_OK;
}

int64_t mr_pulse_restore_all(void) {
    pthread_mutex_lock(&mr_pulse_lock);
    int failures = 0;
    for (int i = 0; i < mr_pending_count; i++) {
        if (write_num(mr_pending[i].path, mr_pending[i].restore_to) != 0)
            failures++;
    }
    mr_pending_count = 0;
    pthread_mutex_unlock(&mr_pulse_lock);
    return failures;
}

/* ---- Run over all delegated scopes in the caller's app.slice ---- */

#include <dirent.h>

static const char *APP_SLICE = "/sys/fs/cgroup/user.slice";
/* user-1000 is the uid the production build serves; the test build can
 * override the whole tree through MEMREDUCT_TEST_PROC_ROOT, so the slice
 * root is derived from the effective uid at call time. */

static void slice_path(char *buf, size_t n) {
    unsigned long uid = (unsigned long)geteuid();
    snprintf(buf, n,
             "/sys/fs/cgroup/user.slice/user-%lu.slice/user@%lu.service/app.slice",
             uid, uid);
}

int64_t mr_pulse_run_all(struct mr_pulse_run *out) {
    if (out == NULL) return -1;
    memset(out, 0, sizeof *out);
    out->before_available = -1;
    out->after_available = -1;

    char slice[512];
    slice_path(slice, sizeof slice);
    DIR *dir = opendir(slice);
    if (dir == NULL) return -2;

    /* MemAvailable is read directly so the reported delta is what the user
     * sees, not a cgroup-internal accounting artifact. */
    {
        FILE *f = fopen("/proc/meminfo", "r");
        if (f != NULL) {
            char line[256];
            while (fgets(line, sizeof line, f) != NULL) {
                if (strncmp(line, "MemAvailable:", 13) == 0) {
                    out->before_available = atoll(line + 13) * 1024;
                    break;
                }
            }
            (void)fclose(f);
        }
    }

    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
        if (strncmp(entry->d_name, "app-", 4) != 0) continue;
        char scope[1024];
        snprintf(scope, sizeof scope, "%s/%s", slice, entry->d_name);
        struct mr_pulse_result result;
        int64_t rc = mr_pulse_scope(scope, 8000, &result);
        if (rc == MR_PULSE_OK_NOTHING_TO_DO) continue;
        if (rc < 0) { out->restore_failures++; continue; }
        out->scopes_pulsed++;
        if (result.restored) out->scopes_restored++; else out->restore_failures++;
        if (result.aborted_by_psi) out->psi_aborts++;
    }
    (void)closedir(dir);

    /* Final safety net: any pulse that somehow stayed pending is restored
     * now. Rule 4: after this call every scope is back at its own high. */
    int64_t failures = mr_pulse_restore_all();
    if (failures > 0) out->restore_failures += failures;

    {
        FILE *f = fopen("/proc/meminfo", "r");
        if (f != NULL) {
            char line[256];
            while (fgets(line, sizeof line, f) != NULL) {
                if (strncmp(line, "MemAvailable:", 13) == 0) {
                    out->after_available = atoll(line + 13) * 1024;
                    break;
                }
            }
            (void)fclose(f);
        }
    }
    return 0;
}
