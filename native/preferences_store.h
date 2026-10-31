/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef MEMREDUCT_PREFERENCES_STORE_H
#define MEMREDUCT_PREFERENCES_STORE_H

#include <stdint.h>

enum mr_preferences_language {
    MR_PREFERENCES_EN = 0,
    MR_PREFERENCES_ZH_CN = 1
};

enum mr_preferences_theme {
    MR_PREFERENCES_LIGHT = 0,
    MR_PREFERENCES_DARK = 1
};

enum mr_preferences_load_status {
    MR_PREFERENCES_ABSENT = 0,
    MR_PREFERENCES_VALID = 1,
    MR_PREFERENCES_CORRUPT = 2,
    MR_PREFERENCES_UNREADABLE = 3,
    MR_PREFERENCES_UNSUPPORTED_SCHEMA = 4,
    MR_PREFERENCES_UNSAFE_OBJECT = 5,
    MR_PREFERENCES_OWNERSHIP_OR_MODE = 6,
    MR_PREFERENCES_PATH_UNAVAILABLE = 7
};

enum mr_preferences_store_status {
    MR_PREFERENCES_COMMITTED_DURABLE = 0,
    MR_PREFERENCES_NO_CHANGE = 1,
    MR_PREFERENCES_FAILED_UNCHANGED = 2,
    MR_PREFERENCES_BLOCKED_RELOAD = 3,
    MR_PREFERENCES_VISIBLE_DURABILITY_UNCERTAIN = 4,
    MR_PREFERENCES_RECOVERY_REQUIRED = 5,
    MR_PREFERENCES_INVALID_ARGUMENT = 6
};

enum mr_preferences_store_stage {
    MR_PREFERENCES_STAGE_NONE = 0,
    MR_PREFERENCES_STAGE_ARGUMENT = 1,
    MR_PREFERENCES_STAGE_CONFIG_ROOT = 2,
    MR_PREFERENCES_STAGE_APP_DIRECTORY = 3,
    MR_PREFERENCES_STAGE_LOCK = 4,
    MR_PREFERENCES_STAGE_RELOAD = 5,
    MR_PREFERENCES_STAGE_TEMP_CREATE = 6,
    MR_PREFERENCES_STAGE_WRITE = 7,
    MR_PREFERENCES_STAGE_FILE_FSYNC = 8,
    MR_PREFERENCES_STAGE_FILE_CLOSE = 9,
    MR_PREFERENCES_STAGE_PRE_RENAME_CHECK = 10,
    MR_PREFERENCES_STAGE_RENAME = 11,
    MR_PREFERENCES_STAGE_DIRECTORY_FSYNC = 12,
    MR_PREFERENCES_STAGE_READBACK = 13,
    MR_PREFERENCES_STAGE_COMPLETE = 14
};

enum mr_preferences_store_error {
    MR_PREFERENCES_ERROR_NONE = 0,
    MR_PREFERENCES_ERROR_INVALID_ARGUMENT = 1,
    MR_PREFERENCES_ERROR_PATH = 2,
    MR_PREFERENCES_ERROR_UNSAFE_OBJECT = 3,
    MR_PREFERENCES_ERROR_PERMISSION = 4,
    MR_PREFERENCES_ERROR_FORMAT = 5,
    MR_PREFERENCES_ERROR_NO_SPACE = 6,
    MR_PREFERENCES_ERROR_IO = 7,
    MR_PREFERENCES_ERROR_CONFLICT = 8
};

/* Scalar FFI. Values are en/zh-CN = 0/1 and light/dark = 0/1. */
int64_t mr_preferences_load(void);
int64_t mr_preferences_language(void);
int64_t mr_preferences_theme(void);
int64_t mr_preferences_values_known(void);
int64_t mr_preferences_can_save(void);
int64_t mr_preferences_set_language(int64_t language);
int64_t mr_preferences_set_theme(int64_t theme);
int64_t mr_preferences_reset(void);
int64_t mr_preferences_last_stage(void);
int64_t mr_preferences_last_error(void);
/* rename: 1 definitely succeeded, 0 did not succeed, -1 outcome unknown. */
int64_t mr_preferences_last_rename_succeeded(void);
/* LoadStatus from a safe readback, or -1 when no readback was needed. */
int64_t mr_preferences_last_readback_status(void);

#ifdef MR_PREFERENCES_TESTING
enum mr_preferences_test_fault {
    MR_PREFERENCES_TEST_NO_FAULT = 0,
    MR_PREFERENCES_TEST_WRITE_EINTR = 1,
    MR_PREFERENCES_TEST_WRITE_SHORT = 2,
    MR_PREFERENCES_TEST_WRITE_FAIL = 3,
    MR_PREFERENCES_TEST_FILE_FSYNC_FAIL = 4,
    MR_PREFERENCES_TEST_RENAME_BEFORE = 5,
    MR_PREFERENCES_TEST_RENAME_AFTER = 6,
    MR_PREFERENCES_TEST_DIR_FSYNC_FAIL = 7,
    MR_PREFERENCES_TEST_OPENAT2_ENOSYS = 8
};

/* Test-only, one-shot fault injection; absent from production builds. */
int64_t mr_preferences_test_inject_fault(int64_t fault);
#endif

#endif
