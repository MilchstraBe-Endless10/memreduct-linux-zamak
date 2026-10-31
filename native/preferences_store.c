/* SPDX-License-Identifier: GPL-3.0-or-later */
#define _GNU_SOURCE
#define _POSIX_C_SOURCE 200809L

#include "preferences_store.h"

#include <errno.h>
#include <fcntl.h>
#include <linux/openat2.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/file.h>
#include <sys/random.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

#if defined(MR_PREFERENCES_TESTING) && !defined(MR_PREFERENCES_TEST_ROOT)
#error "MR_PREFERENCES_TESTING requires a compile-time MR_PREFERENCES_TEST_ROOT"
#endif

#define APP_DIR_NAME "memreduct-zamak"
#define CONFIG_NAME "preferences.conf"
#define LOCK_NAME "preferences.lock"
#define TEMP_PREFIX ".preferences.tmp."
#define MAX_CONFIG_BYTES 4096U
#define TEMP_ATTEMPTS 16

struct preferences {
    int64_t language;
    int64_t theme;
};

struct config_snapshot {
    int64_t status;
    int exists;
    struct preferences value;
    size_t length;
    unsigned char bytes[MAX_CONFIG_BYTES + 1];
};

struct store_state {
    struct preferences value;
    int values_known;
    int can_save;
    int pending_directory_sync;
    int pending_rename_succeeded;
    size_t pending_length;
    unsigned char pending_bytes[MAX_CONFIG_BYTES];
};

static struct store_state state = {
    .value = {MR_PREFERENCES_EN, MR_PREFERENCES_LIGHT},
    .values_known = 1,
    .can_save = 1
};

static int64_t last_store_stage = MR_PREFERENCES_STAGE_NONE;
static int64_t last_store_error = MR_PREFERENCES_ERROR_NONE;
static int64_t last_rename_succeeded = 0;
static int64_t last_readback_status = -1;

static int64_t finish_store(int64_t status, int64_t stage, int64_t error,
                            int64_t rename_succeeded, int64_t readback_status) {
    last_store_stage = stage;
    last_store_error = error;
    last_rename_succeeded = rename_succeeded;
    last_readback_status = readback_status;
    return status;
}

static int64_t error_from_load_status(int64_t status) {
    switch (status) {
        case MR_PREFERENCES_CORRUPT:
        case MR_PREFERENCES_UNSUPPORTED_SCHEMA: return MR_PREFERENCES_ERROR_FORMAT;
        case MR_PREFERENCES_UNSAFE_OBJECT: return MR_PREFERENCES_ERROR_UNSAFE_OBJECT;
        case MR_PREFERENCES_OWNERSHIP_OR_MODE: return MR_PREFERENCES_ERROR_PERMISSION;
        case MR_PREFERENCES_UNREADABLE: return MR_PREFERENCES_ERROR_IO;
        default: return MR_PREFERENCES_ERROR_PATH;
    }
}

static int64_t error_from_errno(int error_number) {
    if (error_number == EACCES || error_number == EPERM) return MR_PREFERENCES_ERROR_PERMISSION;
    if (error_number == ELOOP || error_number == EISDIR || error_number == ENOTDIR)
        return MR_PREFERENCES_ERROR_UNSAFE_OBJECT;
    if (error_number == ENOSPC
#ifdef EDQUOT
        || error_number == EDQUOT
#endif
    ) return MR_PREFERENCES_ERROR_NO_SPACE;
    if (error_number == EINVAL || error_number == ENOSYS || error_number == EXDEV)
        return MR_PREFERENCES_ERROR_PATH;
    return MR_PREFERENCES_ERROR_IO;
}

#ifdef MR_PREFERENCES_TESTING
static int64_t next_test_fault = MR_PREFERENCES_TEST_NO_FAULT;

int64_t mr_preferences_test_inject_fault(int64_t fault) {
    if (fault < MR_PREFERENCES_TEST_NO_FAULT || fault > MR_PREFERENCES_TEST_OPENAT2_ENOSYS)
        return MR_PREFERENCES_INVALID_ARGUMENT;
    next_test_fault = fault;
    return MR_PREFERENCES_COMMITTED_DURABLE;
}

static int consume_fault(int64_t expected) {
    if (next_test_fault != expected) return 0;
    next_test_fault = MR_PREFERENCES_TEST_NO_FAULT;
    return 1;
}
#endif

static int path_is_lexically_safe(const char *path) {
    if (path == NULL || path[0] != '/') return 0;
    const char *cursor = path + 1;
    while (*cursor != '\0') {
        while (*cursor == '/') cursor++;
        if (*cursor == '\0') break;
        const char *start = cursor;
        while (*cursor != '\0' && *cursor != '/') cursor++;
        size_t length = (size_t)(cursor - start);
        if ((length == 1 && start[0] == '.') ||
            (length == 2 && start[0] == '.' && start[1] == '.'))
            return 0;
    }
    return 1;
}

static int resolve_config_root(char *buffer, size_t capacity) {
#ifdef MR_PREFERENCES_TESTING
    const char *root = MR_PREFERENCES_TEST_ROOT;
    if (!path_is_lexically_safe(root)) return -1;
    int count = snprintf(buffer, capacity, "%s", root);
    return count > 0 && (size_t)count < capacity ? 0 : -1;
#else
    const char *xdg = getenv("XDG_CONFIG_HOME");
    if (xdg != NULL && xdg[0] != '\0' && xdg[0] == '/') {
        if (!path_is_lexically_safe(xdg)) return -1;
        int count = snprintf(buffer, capacity, "%s", xdg);
        return count > 0 && (size_t)count < capacity ? 0 : -1;
    }

    const char *home = getenv("HOME");
    if (home == NULL || home[0] == '\0' || !path_is_lexically_safe(home)) return -1;
    int count = snprintf(buffer, capacity, "%s/.config", home);
    return count > 0 && (size_t)count < capacity ? 0 : -1;
#endif
}

/* Use openat2 where present. Only ENOSYS permits the fixed-basename openat fallback. */
static int safe_openat(int directory_fd, const char *name, int flags, mode_t mode) {
#ifdef SYS_openat2
    struct open_how how;
    memset(&how, 0, sizeof(how));
    how.flags = (uint64_t)flags;
    how.mode = (flags & (O_CREAT | O_TMPFILE)) != 0 ? (uint64_t)mode : 0;
    how.resolve = RESOLVE_BENEATH | RESOLVE_NO_MAGICLINKS | RESOLVE_NO_SYMLINKS;
    int fd;
#ifdef MR_PREFERENCES_TESTING
    if (consume_fault(MR_PREFERENCES_TEST_OPENAT2_ENOSYS)) {
        errno = ENOSYS;
        fd = -1;
    } else
#endif
        fd = (int)syscall(SYS_openat2, directory_fd, name, &how, sizeof(how));
    if (fd >= 0 || errno != ENOSYS) return fd;
#else
    errno = ENOSYS;
    return -1;
#endif
    return openat(directory_fd, name, flags | O_CLOEXEC | O_NOFOLLOW, mode);
}

static int sync_fd(int fd) {
#ifdef MR_PREFERENCES_TESTING
    struct stat info;
    if (fstat(fd, &info) == 0) {
        if (S_ISREG(info.st_mode) && consume_fault(MR_PREFERENCES_TEST_FILE_FSYNC_FAIL)) {
            errno = EIO;
            return -1;
        }
        if (S_ISDIR(info.st_mode) && consume_fault(MR_PREFERENCES_TEST_DIR_FSYNC_FAIL)) {
            errno = EIO;
            return -1;
        }
    }
#endif
    int result;
    do {
        result = fsync(fd);
    } while (result == -1 && errno == EINTR);
    return result;
}

static ssize_t write_bytes(int fd, const void *bytes, size_t length) {
#ifdef MR_PREFERENCES_TESTING
    if (consume_fault(MR_PREFERENCES_TEST_WRITE_EINTR)) {
        errno = EINTR;
        return -1;
    }
    if (consume_fault(MR_PREFERENCES_TEST_WRITE_FAIL)) {
        errno = EIO;
        return -1;
    }
    if (length > 1 && consume_fault(MR_PREFERENCES_TEST_WRITE_SHORT))
        return write(fd, bytes, length / 2);
#endif
    return write(fd, bytes, length);
}

static int rename_config(int directory_fd, const char *temporary_name) {
#ifdef MR_PREFERENCES_TESTING
    if (consume_fault(MR_PREFERENCES_TEST_RENAME_BEFORE)) {
        errno = EIO;
        return -1;
    }
    if (consume_fault(MR_PREFERENCES_TEST_RENAME_AFTER)) {
        if (renameat(directory_fd, temporary_name, directory_fd, CONFIG_NAME) == -1)
            return -1;
        errno = EIO;
        return -1;
    }
#endif
    return renameat(directory_fd, temporary_name, directory_fd, CONFIG_NAME);
}

static int open_config_root(int *root_fd) {
    char path[4096];
    if (resolve_config_root(path, sizeof(path)) == -1) return MR_PREFERENCES_PATH_UNAVAILABLE;

    int fd = open(path, O_RDONLY | O_DIRECTORY | O_CLOEXEC);
    if (fd == -1) return MR_PREFERENCES_PATH_UNAVAILABLE;

    struct stat info;
    if (fstat(fd, &info) == -1) {
        (void)close(fd);
        return MR_PREFERENCES_PATH_UNAVAILABLE;
    }
    if (!S_ISDIR(info.st_mode)) {
        (void)close(fd);
        return MR_PREFERENCES_UNSAFE_OBJECT;
    }
    if (info.st_uid != geteuid() || (info.st_mode & (S_IWGRP | S_IWOTH)) != 0) {
        (void)close(fd);
        return MR_PREFERENCES_OWNERSHIP_OR_MODE;
    }
    *root_fd = fd;
    return MR_PREFERENCES_VALID;
}

static int directory_metadata_status(const struct stat *info, int app_directory) {
    if (!S_ISDIR(info->st_mode)) return MR_PREFERENCES_UNSAFE_OBJECT;
    if (info->st_uid != geteuid()) return MR_PREFERENCES_OWNERSHIP_OR_MODE;
    mode_t mode = info->st_mode & 07777;
    if (app_directory ? mode != 0700 : (mode & (S_IWGRP | S_IWOTH)) != 0)
        return MR_PREFERENCES_OWNERSHIP_OR_MODE;
    return MR_PREFERENCES_VALID;
}

static int open_app_directory(int root_fd, int create, int *app_fd) {
    for (int attempt = 0; attempt < 4; attempt++) {
        struct stat before;
        if (fstatat(root_fd, APP_DIR_NAME, &before, AT_SYMLINK_NOFOLLOW) == 0) {
            int status = directory_metadata_status(&before, 1);
            if (status != MR_PREFERENCES_VALID) return status;
            int fd = safe_openat(root_fd, APP_DIR_NAME,
                                 O_RDONLY | O_DIRECTORY | O_CLOEXEC | O_NOFOLLOW, 0);
            if (fd == -1) {
                if (errno == ENOENT) continue;
                return errno == ELOOP ? MR_PREFERENCES_UNSAFE_OBJECT : MR_PREFERENCES_PATH_UNAVAILABLE;
            }
            struct stat after;
            if (fstat(fd, &after) == -1 || before.st_dev != after.st_dev || before.st_ino != after.st_ino) {
                (void)close(fd);
                return MR_PREFERENCES_UNSAFE_OBJECT;
            }
            status = directory_metadata_status(&after, 1);
            if (status != MR_PREFERENCES_VALID) {
                (void)close(fd);
                return status;
            }
            *app_fd = fd;
            return MR_PREFERENCES_VALID;
        }

        if (errno != ENOENT) return MR_PREFERENCES_PATH_UNAVAILABLE;
        if (!create) return MR_PREFERENCES_ABSENT;

        if (mkdirat(root_fd, APP_DIR_NAME, 0700) == -1) {
            if (errno == EEXIST || errno == EINTR) continue;
            return MR_PREFERENCES_PATH_UNAVAILABLE;
        }

        struct stat created;
        if (fstatat(root_fd, APP_DIR_NAME, &created, AT_SYMLINK_NOFOLLOW) == -1 ||
            !S_ISDIR(created.st_mode) || created.st_uid != geteuid())
            return MR_PREFERENCES_UNSAFE_OBJECT;
        int fd = safe_openat(root_fd, APP_DIR_NAME,
                             O_RDONLY | O_DIRECTORY | O_CLOEXEC | O_NOFOLLOW, 0);
        if (fd == -1) return errno == ELOOP ? MR_PREFERENCES_UNSAFE_OBJECT : MR_PREFERENCES_PATH_UNAVAILABLE;
        struct stat opened;
        if (fstat(fd, &opened) == -1 || created.st_dev != opened.st_dev || created.st_ino != opened.st_ino) {
            (void)close(fd);
            return MR_PREFERENCES_UNSAFE_OBJECT;
        }
        if (fchmod(fd, 0700) == -1 || fstat(fd, &opened) == -1) {
            (void)close(fd);
            return MR_PREFERENCES_PATH_UNAVAILABLE;
        }
        int status = directory_metadata_status(&opened, 1);
        if (status != MR_PREFERENCES_VALID) {
            (void)close(fd);
            return status;
        }
        if (sync_fd(root_fd) == -1) {
            (void)close(fd);
            return MR_PREFERENCES_PATH_UNAVAILABLE;
        }
        *app_fd = fd;
        return MR_PREFERENCES_VALID;
    }
    return MR_PREFERENCES_PATH_UNAVAILABLE;
}

static int leaf_metadata_status(const struct stat *info) {
    if (!S_ISREG(info->st_mode) || info->st_nlink != 1) return MR_PREFERENCES_UNSAFE_OBJECT;
    if (info->st_uid != geteuid() || (info->st_mode & 07777) != 0600)
        return MR_PREFERENCES_OWNERSHIP_OR_MODE;
    return MR_PREFERENCES_VALID;
}

static int open_lock_file(int app_fd, int *lock_fd) {
    for (int attempt = 0; attempt < 8; attempt++) {
        struct stat before;
        if (fstatat(app_fd, LOCK_NAME, &before, AT_SYMLINK_NOFOLLOW) == -1) {
            if (errno != ENOENT) return errno == EACCES ? MR_PREFERENCES_OWNERSHIP_OR_MODE : MR_PREFERENCES_UNSAFE_OBJECT;
            int fd = safe_openat(app_fd, LOCK_NAME,
                                 O_CREAT | O_EXCL | O_RDWR | O_CLOEXEC | O_NOFOLLOW, 0600);
            if (fd == -1) {
                if (errno == EEXIST || errno == EINTR) continue;
                return errno == EACCES ? MR_PREFERENCES_OWNERSHIP_OR_MODE : MR_PREFERENCES_PATH_UNAVAILABLE;
            }
            struct stat created;
            if (fstat(fd, &created) == -1 || !S_ISREG(created.st_mode) ||
                created.st_uid != geteuid() || created.st_nlink != 1 ||
                fchmod(fd, 0600) == -1 || fstat(fd, &created) == -1 ||
                leaf_metadata_status(&created) != MR_PREFERENCES_VALID) {
                (void)close(fd);
                return MR_PREFERENCES_UNSAFE_OBJECT;
            }
            if (sync_fd(app_fd) == -1) {
                (void)close(fd);
                return MR_PREFERENCES_PATH_UNAVAILABLE;
            }
            *lock_fd = fd;
            break;
        }

        int status = leaf_metadata_status(&before);
        if (status != MR_PREFERENCES_VALID) return status;
        int fd = safe_openat(app_fd, LOCK_NAME, O_RDWR | O_CLOEXEC | O_NOFOLLOW, 0);
        if (fd == -1) {
            if (errno == ENOENT) continue;
            return errno == ELOOP ? MR_PREFERENCES_UNSAFE_OBJECT :
                   errno == EACCES ? MR_PREFERENCES_OWNERSHIP_OR_MODE : MR_PREFERENCES_PATH_UNAVAILABLE;
        }
        struct stat opened;
        if (fstat(fd, &opened) == -1 || before.st_dev != opened.st_dev || before.st_ino != opened.st_ino) {
            (void)close(fd);
            return MR_PREFERENCES_UNSAFE_OBJECT;
        }
        status = leaf_metadata_status(&opened);
        if (status != MR_PREFERENCES_VALID) {
            (void)close(fd);
            return status;
        }
        *lock_fd = fd;
        break;
    }

    if (*lock_fd < 0) return MR_PREFERENCES_PATH_UNAVAILABLE;
    int lock_result;
    do {
        lock_result = flock(*lock_fd, LOCK_EX);
    } while (lock_result == -1 && errno == EINTR);
    if (lock_result == -1) {
        (void)close(*lock_fd);
        *lock_fd = -1;
        return MR_PREFERENCES_PATH_UNAVAILABLE;
    }
    struct stat locked;
    if (fstat(*lock_fd, &locked) == -1 || leaf_metadata_status(&locked) != MR_PREFERENCES_VALID) {
        (void)flock(*lock_fd, LOCK_UN);
        (void)close(*lock_fd);
        *lock_fd = -1;
        return MR_PREFERENCES_OWNERSHIP_OR_MODE;
    }
    struct stat lock_path;
    if (fstatat(app_fd, LOCK_NAME, &lock_path, AT_SYMLINK_NOFOLLOW) == -1 ||
        lock_path.st_dev != locked.st_dev || lock_path.st_ino != locked.st_ino ||
        leaf_metadata_status(&lock_path) != MR_PREFERENCES_VALID) {
        (void)flock(*lock_fd, LOCK_UN);
        (void)close(*lock_fd);
        *lock_fd = -1;
        return MR_PREFERENCES_UNSAFE_OBJECT;
    }
    return MR_PREFERENCES_VALID;
}

static int same_file_metadata(const struct stat *left, const struct stat *right) {
    return left->st_dev == right->st_dev && left->st_ino == right->st_ino &&
           left->st_uid == right->st_uid && left->st_mode == right->st_mode &&
           left->st_nlink == right->st_nlink && left->st_size == right->st_size &&
           left->st_mtim.tv_sec == right->st_mtim.tv_sec &&
           left->st_mtim.tv_nsec == right->st_mtim.tv_nsec &&
           left->st_ctim.tv_sec == right->st_ctim.tv_sec &&
           left->st_ctim.tv_nsec == right->st_ctim.tv_nsec;
}

static int valid_key(const unsigned char *key, size_t length) {
    if (length == 0) return 0;
    for (size_t i = 0; i < length; i++) {
        unsigned char c = key[i];
        if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
              (c >= '0' && c <= '9') || c == '_')) return 0;
    }
    return 1;
}

static int parse_schema(const unsigned char *value, size_t length, int *future) {
    if (length == 0) return -1;
    uint64_t schema = 0;
    for (size_t i = 0; i < length; i++) {
        if (value[i] < '0' || value[i] > '9') return -1;
        unsigned digit = (unsigned)(value[i] - '0');
        if (schema > (UINT64_MAX - digit) / 10) {
            *future = 1;
            continue;
        }
        schema = schema * 10 + digit;
    }
    if (schema != 1) *future = 1;
    return 0;
}

static int parse_preferences(const unsigned char *bytes, size_t length, struct preferences *value) {
    int seen_schema = 0;
    int seen_language = 0;
    int seen_theme = 0;
    int future_schema = 0;
    int invalid_known_fields = 0;
    int64_t language = MR_PREFERENCES_EN;
    int64_t theme = MR_PREFERENCES_LIGHT;

    for (size_t i = 0; i < length; i++) {
        unsigned char c = bytes[i];
        if (c != '\n' && (c < 0x20 || c > 0x7e)) return MR_PREFERENCES_CORRUPT;
    }

    size_t position = 0;
    while (position < length) {
        size_t end = position;
        while (end < length && bytes[end] != '\n') end++;
        size_t line_length = end - position;
        if (line_length == 0) return MR_PREFERENCES_CORRUPT;
        size_t equals = position;
        while (equals < end && bytes[equals] != '=') equals++;
        if (equals == position || equals == end || !valid_key(bytes + position, equals - position))
            return MR_PREFERENCES_CORRUPT;
        for (size_t j = equals + 1; j < end; j++)
            if (bytes[j] == '=') return MR_PREFERENCES_CORRUPT;
        size_t key_length = equals - position;
        const unsigned char *field = bytes + position;
        const unsigned char *field_value = bytes + equals + 1;
        size_t value_length = end - equals - 1;
        if (value_length == 0) return MR_PREFERENCES_CORRUPT;

        if (key_length == 6 && memcmp(field, "schema", 6) == 0) {
            if (seen_schema || parse_schema(field_value, value_length, &future_schema) == -1)
                return MR_PREFERENCES_CORRUPT;
            seen_schema = 1;
        } else if (key_length == 8 && memcmp(field, "language", 8) == 0) {
            if (seen_language) {
                invalid_known_fields = 1;
            } else {
                seen_language = 1;
                if (value_length == 2 && memcmp(field_value, "en", 2) == 0)
                    language = MR_PREFERENCES_EN;
                else if (value_length == 5 && memcmp(field_value, "zh-CN", 5) == 0)
                    language = MR_PREFERENCES_ZH_CN;
                else invalid_known_fields = 1;
            }
        } else if (key_length == 5 && memcmp(field, "theme", 5) == 0) {
            if (seen_theme) {
                invalid_known_fields = 1;
            } else {
                seen_theme = 1;
                if (value_length == 5 && memcmp(field_value, "light", 5) == 0)
                    theme = MR_PREFERENCES_LIGHT;
                else if (value_length == 4 && memcmp(field_value, "dark", 4) == 0)
                    theme = MR_PREFERENCES_DARK;
                else invalid_known_fields = 1;
            }
        }

        position = end < length ? end + 1 : length;
    }

    if (!seen_schema) return MR_PREFERENCES_CORRUPT;
    if (future_schema) return MR_PREFERENCES_UNSUPPORTED_SCHEMA;
    if (invalid_known_fields || !seen_language || !seen_theme) return MR_PREFERENCES_CORRUPT;
    value->language = language;
    value->theme = theme;
    return MR_PREFERENCES_VALID;
}

static int read_config(int app_fd, struct config_snapshot *snapshot) {
    memset(snapshot, 0, sizeof(*snapshot));
    snapshot->status = MR_PREFERENCES_UNREADABLE;

    struct stat before;
    if (fstatat(app_fd, CONFIG_NAME, &before, AT_SYMLINK_NOFOLLOW) == -1) {
        if (errno == ENOENT) {
            snapshot->status = MR_PREFERENCES_ABSENT;
            return snapshot->status;
        }
        snapshot->status = errno == EACCES ? MR_PREFERENCES_UNREADABLE : MR_PREFERENCES_PATH_UNAVAILABLE;
        return snapshot->status;
    }
    snapshot->exists = 1;
    if (!S_ISREG(before.st_mode) || before.st_nlink != 1) {
        snapshot->status = MR_PREFERENCES_UNSAFE_OBJECT;
        return snapshot->status;
    }
    if (before.st_uid != geteuid() || (before.st_mode & 07777) != 0600) {
        snapshot->status = MR_PREFERENCES_OWNERSHIP_OR_MODE;
        return snapshot->status;
    }

    int fd = safe_openat(app_fd, CONFIG_NAME, O_RDONLY | O_CLOEXEC | O_NOFOLLOW | O_NONBLOCK, 0);
    if (fd == -1) {
        snapshot->status = errno == ELOOP ? MR_PREFERENCES_UNSAFE_OBJECT :
                           errno == EACCES ? MR_PREFERENCES_UNREADABLE : MR_PREFERENCES_PATH_UNAVAILABLE;
        return snapshot->status;
    }
    struct stat opened;
    if (fstat(fd, &opened) == -1) {
        (void)close(fd);
        snapshot->status = MR_PREFERENCES_UNREADABLE;
        return snapshot->status;
    }
    int metadata_status = leaf_metadata_status(&opened);
    if (metadata_status != MR_PREFERENCES_VALID) {
        (void)close(fd);
        snapshot->status = metadata_status;
        return snapshot->status;
    }
    if (opened.st_size < 0 || (uint64_t)opened.st_size > MAX_CONFIG_BYTES) {
        (void)close(fd);
        snapshot->status = MR_PREFERENCES_CORRUPT;
        return snapshot->status;
    }

    size_t total = 0;
    while (total < sizeof(snapshot->bytes)) {
        ssize_t count = read(fd, snapshot->bytes + total, sizeof(snapshot->bytes) - total);
        if (count == -1 && errno == EINTR) continue;
        if (count == -1) {
            (void)close(fd);
            snapshot->status = MR_PREFERENCES_UNREADABLE;
            return snapshot->status;
        }
        if (count == 0) break;
        total += (size_t)count;
    }
    struct stat after;
    int stat_result = fstat(fd, &after);
    int close_result = close(fd);
    if (stat_result == -1 || close_result == -1 || !same_file_metadata(&opened, &after)) {
        snapshot->status = MR_PREFERENCES_UNREADABLE;
        return snapshot->status;
    }
    if (total > MAX_CONFIG_BYTES) {
        snapshot->status = MR_PREFERENCES_CORRUPT;
        return snapshot->status;
    }
    snapshot->length = total;
    snapshot->status = parse_preferences(snapshot->bytes, total, &snapshot->value);
    return snapshot->status;
}

static size_t serialize_preferences(const struct preferences *value, unsigned char *buffer, size_t capacity) {
    const char *language = value->language == MR_PREFERENCES_ZH_CN ? "zh-CN" : "en";
    const char *theme = value->theme == MR_PREFERENCES_DARK ? "dark" : "light";
    int count = snprintf((char *)buffer, capacity, "schema=1\nlanguage=%s\ntheme=%s\n", language, theme);
    return count > 0 && (size_t)count < capacity ? (size_t)count : 0;
}

static int random_bytes(unsigned char *buffer, size_t length) {
    size_t total = 0;
    while (total < length) {
        ssize_t count = getrandom(buffer + total, length - total, 0);
        if (count == -1 && errno == EINTR) continue;
        if (count <= 0) return -1;
        total += (size_t)count;
    }
    return 0;
}

static int create_temp(int app_fd, char *name, size_t name_capacity, int *temp_fd,
                       struct stat *created_stat) {
    static const char hex[] = "0123456789abcdef";
    unsigned char random[16];
    for (int attempt = 0; attempt < TEMP_ATTEMPTS; attempt++) {
        if (random_bytes(random, sizeof(random)) == -1) return -1;
        size_t prefix_length = sizeof(TEMP_PREFIX) - 1;
        if (prefix_length + sizeof(random) * 2 + 1 > name_capacity) {
            errno = ENAMETOOLONG;
            return -1;
        }
        memcpy(name, TEMP_PREFIX, prefix_length);
        for (size_t i = 0; i < sizeof(random); i++) {
            name[prefix_length + i * 2] = hex[random[i] >> 4];
            name[prefix_length + i * 2 + 1] = hex[random[i] & 0x0f];
        }
        name[prefix_length + sizeof(random) * 2] = '\0';
        int fd = safe_openat(app_fd, name,
                             O_WRONLY | O_CREAT | O_EXCL | O_CLOEXEC | O_NOFOLLOW, 0600);
        if (fd == -1) {
            if (errno == EEXIST) continue;
            return -1;
        }
        if (fstat(fd, created_stat) == -1 || !S_ISREG(created_stat->st_mode) ||
            created_stat->st_uid != geteuid() || created_stat->st_nlink != 1 ||
            fchmod(fd, 0600) == -1 || fstat(fd, created_stat) == -1 ||
            leaf_metadata_status(created_stat) != MR_PREFERENCES_VALID) {
            int saved_errno = errno == 0 ? EIO : errno;
            (void)close(fd);
            errno = saved_errno;
            return -1;
        }
        *temp_fd = fd;
        return 0;
    }
    errno = EEXIST;
    return -1;
}

static void remove_temp_if_owned(int app_fd, const char *name, const struct stat *created) {
    if (name[0] == '\0') return;
    struct stat current;
    if (fstatat(app_fd, name, &current, AT_SYMLINK_NOFOLLOW) == -1) return;
    if (current.st_dev == created->st_dev && current.st_ino == created->st_ino &&
        current.st_uid == geteuid() && current.st_nlink == 1 && S_ISREG(current.st_mode) &&
        (current.st_mode & 07777) == 0600)
        (void)unlinkat(app_fd, name, 0);
}

static int write_temp(int app_fd, const unsigned char *bytes, size_t length,
                      char *name, size_t name_capacity, struct stat *created,
                      int64_t *failed_stage) {
    int fd = -1;
    name[0] = '\0';
    if (create_temp(app_fd, name, name_capacity, &fd, created) == -1) {
        *failed_stage = MR_PREFERENCES_STAGE_TEMP_CREATE;
        return -1;
    }
    size_t written = 0;
    while (written < length) {
        ssize_t count = write_bytes(fd, bytes + written, length - written);
        if (count == -1 && errno == EINTR) continue;
        if (count <= 0) {
            *failed_stage = MR_PREFERENCES_STAGE_WRITE;
            int saved_errno = count == 0 ? EIO : errno;
            (void)close(fd);
            remove_temp_if_owned(app_fd, name, created);
            errno = saved_errno;
            return -1;
        }
        written += (size_t)count;
    }
    if (sync_fd(fd) == -1) {
        *failed_stage = MR_PREFERENCES_STAGE_FILE_FSYNC;
        int saved_errno = errno;
        (void)close(fd);
        remove_temp_if_owned(app_fd, name, created);
        errno = saved_errno;
        return -1;
    }
    if (close(fd) == -1) {
        *failed_stage = MR_PREFERENCES_STAGE_FILE_CLOSE;
        int saved_errno = errno;
        remove_temp_if_owned(app_fd, name, created);
        errno = saved_errno;
        return -1;
    }
    return 0;
}

static int snapshot_unchanged(const struct config_snapshot *before,
                              const struct config_snapshot *after) {
    if (before->status != after->status || before->exists != after->exists) return 0;
    if (!before->exists) return before->status == MR_PREFERENCES_ABSENT;
    return before->status == MR_PREFERENCES_VALID && after->status == MR_PREFERENCES_VALID &&
           before->length == after->length &&
           memcmp(before->bytes, after->bytes, before->length) == 0;
}

static int snapshot_is_expected(const struct config_snapshot *snapshot,
                                const unsigned char *expected, size_t expected_length) {
    return snapshot->status == MR_PREFERENCES_VALID && snapshot->exists &&
           snapshot->length == expected_length &&
           memcmp(snapshot->bytes, expected, expected_length) == 0;
}

static void adopt_snapshot(const struct config_snapshot *snapshot, int allow_save) {
    if (snapshot->status == MR_PREFERENCES_VALID) {
        state.value = snapshot->value;
        state.values_known = 1;
        state.can_save = allow_save;
    } else if (snapshot->status == MR_PREFERENCES_ABSENT) {
        state.value.language = MR_PREFERENCES_EN;
        state.value.theme = MR_PREFERENCES_LIGHT;
        state.values_known = 1;
        state.can_save = allow_save;
    } else {
        state.values_known = 0;
        state.can_save = 0;
    }
}

static void remember_pending_sync(const unsigned char *bytes, size_t length, int rename_succeeded) {
    state.pending_directory_sync = 1;
    state.pending_rename_succeeded = rename_succeeded;
    state.pending_length = length;
    memcpy(state.pending_bytes, bytes, length);
}

static void clear_pending_sync(void) {
    state.pending_directory_sync = 0;
    state.pending_rename_succeeded = 0;
    state.pending_length = 0;
}

static int64_t store_transaction(int field, int64_t requested_value, int reset) {
    int root_fd = -1;
    int app_fd = -1;
    int lock_fd = -1;
    int64_t result = MR_PREFERENCES_FAILED_UNCHANGED;
    int64_t stage = MR_PREFERENCES_STAGE_ARGUMENT;
    int64_t error = MR_PREFERENCES_ERROR_NONE;
    int64_t renamed = 0;
    int64_t readback_status = -1;

    if (!reset && ((field == 0 && requested_value != MR_PREFERENCES_EN && requested_value != MR_PREFERENCES_ZH_CN) ||
                   (field == 1 && requested_value != MR_PREFERENCES_LIGHT && requested_value != MR_PREFERENCES_DARK))) {
        result = MR_PREFERENCES_INVALID_ARGUMENT;
        error = MR_PREFERENCES_ERROR_INVALID_ARGUMENT;
        goto done;
    }

    stage = MR_PREFERENCES_STAGE_CONFIG_ROOT;
    int root_status = open_config_root(&root_fd);
    if (root_status != MR_PREFERENCES_VALID) {
        state.can_save = 0;
        result = MR_PREFERENCES_BLOCKED_RELOAD;
        error = error_from_load_status(root_status);
        goto done;
    }
    stage = MR_PREFERENCES_STAGE_APP_DIRECTORY;
    int app_status = open_app_directory(root_fd, 1, &app_fd);
    if (app_status != MR_PREFERENCES_VALID) {
        state.can_save = 0;
        result = MR_PREFERENCES_BLOCKED_RELOAD;
        error = error_from_load_status(app_status);
        goto done;
    }
    stage = MR_PREFERENCES_STAGE_LOCK;
    int lock_status = open_lock_file(app_fd, &lock_fd);
    if (lock_status != MR_PREFERENCES_VALID) {
        state.can_save = 0;
        result = MR_PREFERENCES_BLOCKED_RELOAD;
        error = error_from_load_status(lock_status);
        goto done;
    }

    stage = MR_PREFERENCES_STAGE_RELOAD;
    struct config_snapshot before;
    int load_status = read_config(app_fd, &before);
    if (load_status != MR_PREFERENCES_VALID && load_status != MR_PREFERENCES_ABSENT) {
        state.can_save = 0;
        result = MR_PREFERENCES_BLOCKED_RELOAD;
        error = error_from_load_status(load_status);
        goto done;
    }

    struct preferences candidate = {MR_PREFERENCES_EN, MR_PREFERENCES_LIGHT};
    if (load_status == MR_PREFERENCES_VALID) candidate = before.value;
    if (reset) {
        candidate.language = MR_PREFERENCES_EN;
        candidate.theme = MR_PREFERENCES_LIGHT;
    } else if (field == 0) {
        candidate.language = requested_value;
    } else {
        candidate.theme = requested_value;
    }

    unsigned char canonical[MAX_CONFIG_BYTES];
    size_t canonical_length = serialize_preferences(&candidate, canonical, sizeof(canonical));
    if (canonical_length == 0) {
        stage = MR_PREFERENCES_STAGE_TEMP_CREATE;
        error = MR_PREFERENCES_ERROR_IO;
        result = MR_PREFERENCES_FAILED_UNCHANGED;
        goto done;
    }
    if (state.pending_directory_sync &&
        snapshot_is_expected(&before, state.pending_bytes, state.pending_length)) {
        if (canonical_length == state.pending_length &&
            memcmp(canonical, state.pending_bytes, canonical_length) == 0) {
            stage = MR_PREFERENCES_STAGE_DIRECTORY_FSYNC;
            renamed = state.pending_rename_succeeded;
            readback_status = before.status;
            if (sync_fd(app_fd) == 0) {
                clear_pending_sync();
                state.value = before.value;
                state.values_known = 1;
                state.can_save = 1;
                stage = MR_PREFERENCES_STAGE_COMPLETE;
                result = MR_PREFERENCES_COMMITTED_DURABLE;
                goto done;
            }
            error = error_from_errno(errno);
            result = MR_PREFERENCES_VISIBLE_DURABILITY_UNCERTAIN;
            goto done;
        }
    } else if (state.pending_directory_sync) {
        clear_pending_sync();
    }
    if (before.status == MR_PREFERENCES_VALID && before.length == canonical_length &&
        memcmp(before.bytes, canonical, canonical_length) == 0) {
        state.value = candidate;
        state.values_known = 1;
        state.can_save = 1;
        stage = MR_PREFERENCES_STAGE_COMPLETE;
        result = MR_PREFERENCES_NO_CHANGE;
        goto done;
    }

    char temporary_name[sizeof(TEMP_PREFIX) + 33];
    struct stat temporary_stat;
    int64_t write_failure_stage = MR_PREFERENCES_STAGE_TEMP_CREATE;
    stage = MR_PREFERENCES_STAGE_TEMP_CREATE;
    if (write_temp(app_fd, canonical, canonical_length, temporary_name,
                   sizeof(temporary_name), &temporary_stat, &write_failure_stage) == -1) {
        stage = write_failure_stage;
        error = error_from_errno(errno);
        result = MR_PREFERENCES_FAILED_UNCHANGED;
        goto done;
    }

    /* Detect non-cooperating edits during the temp write before replacing the name. */
    stage = MR_PREFERENCES_STAGE_PRE_RENAME_CHECK;
    struct config_snapshot pre_rename;
    (void)read_config(app_fd, &pre_rename);
    if (!snapshot_unchanged(&before, &pre_rename)) {
        readback_status = pre_rename.status;
        remove_temp_if_owned(app_fd, temporary_name, &temporary_stat);
        if (pre_rename.status == MR_PREFERENCES_VALID || pre_rename.status == MR_PREFERENCES_ABSENT) {
            adopt_snapshot(&pre_rename, 0);
            result = MR_PREFERENCES_BLOCKED_RELOAD;
            error = MR_PREFERENCES_ERROR_CONFLICT;
        } else {
            state.values_known = 0;
            state.can_save = 0;
            result = MR_PREFERENCES_RECOVERY_REQUIRED;
            error = error_from_load_status(pre_rename.status);
        }
        goto done;
    }

    stage = MR_PREFERENCES_STAGE_RENAME;
    if (rename_config(app_fd, temporary_name) == -1) {
        int rename_error = errno;
        struct config_snapshot after;
        (void)read_config(app_fd, &after);
        readback_status = after.status;
        if (snapshot_unchanged(&before, &after)) {
            remove_temp_if_owned(app_fd, temporary_name, &temporary_stat);
            result = MR_PREFERENCES_FAILED_UNCHANGED;
            error = error_from_errno(rename_error);
            renamed = 0;
        } else if (snapshot_is_expected(&after, canonical, canonical_length)) {
            adopt_snapshot(&after, 1);
            remember_pending_sync(canonical, canonical_length, -1);
            result = MR_PREFERENCES_VISIBLE_DURABILITY_UNCERTAIN;
            error = MR_PREFERENCES_ERROR_IO;
            renamed = -1;
        } else if (after.status == MR_PREFERENCES_VALID || after.status == MR_PREFERENCES_ABSENT) {
            adopt_snapshot(&after, 0);
            clear_pending_sync();
            result = MR_PREFERENCES_VISIBLE_DURABILITY_UNCERTAIN;
            error = MR_PREFERENCES_ERROR_CONFLICT;
            renamed = -1;
        } else {
            state.values_known = 0;
            state.can_save = 0;
            clear_pending_sync();
            result = MR_PREFERENCES_RECOVERY_REQUIRED;
            error = error_from_load_status(after.status);
            stage = MR_PREFERENCES_STAGE_READBACK;
            renamed = -1;
        }
        goto done;
    }

    renamed = 1;
    stage = MR_PREFERENCES_STAGE_DIRECTORY_FSYNC;
    if (sync_fd(app_fd) == -1) {
        int sync_error = errno;
        struct config_snapshot after;
        (void)read_config(app_fd, &after);
        readback_status = after.status;
        if (snapshot_is_expected(&after, canonical, canonical_length)) {
            adopt_snapshot(&after, 1);
            remember_pending_sync(canonical, canonical_length, 1);
            result = MR_PREFERENCES_VISIBLE_DURABILITY_UNCERTAIN;
            error = error_from_errno(sync_error);
        } else if (after.status == MR_PREFERENCES_VALID || after.status == MR_PREFERENCES_ABSENT) {
            adopt_snapshot(&after, 0);
            clear_pending_sync();
            result = MR_PREFERENCES_VISIBLE_DURABILITY_UNCERTAIN;
            error = MR_PREFERENCES_ERROR_CONFLICT;
        } else {
            state.values_known = 0;
            state.can_save = 0;
            clear_pending_sync();
            result = MR_PREFERENCES_RECOVERY_REQUIRED;
            error = error_from_load_status(after.status);
            stage = MR_PREFERENCES_STAGE_READBACK;
        }
        goto done;
    }

    state.value = candidate;
    state.values_known = 1;
    state.can_save = 1;
    clear_pending_sync();
    stage = MR_PREFERENCES_STAGE_COMPLETE;
    result = MR_PREFERENCES_COMMITTED_DURABLE;
    goto done;

done:
    if (lock_fd >= 0) {
        (void)flock(lock_fd, LOCK_UN);
        (void)close(lock_fd);
    }
    if (app_fd >= 0) (void)close(app_fd);
    if (root_fd >= 0) (void)close(root_fd);
    return finish_store(result, stage, error, renamed, readback_status);
}

int64_t mr_preferences_load(void) {
    int root_fd = -1;
    int root_status = open_config_root(&root_fd);
    if (root_status != MR_PREFERENCES_VALID) {
        state.value.language = MR_PREFERENCES_EN;
        state.value.theme = MR_PREFERENCES_LIGHT;
        state.values_known = 1;
        state.can_save = 0;
        return root_status;
    }

    int app_fd = -1;
    int app_status = open_app_directory(root_fd, 0, &app_fd);
    (void)close(root_fd);
    if (app_status == MR_PREFERENCES_ABSENT) {
        state.value.language = MR_PREFERENCES_EN;
        state.value.theme = MR_PREFERENCES_LIGHT;
        state.values_known = 1;
        state.can_save = 1;
        return MR_PREFERENCES_ABSENT;
    }
    if (app_status != MR_PREFERENCES_VALID) {
        state.value.language = MR_PREFERENCES_EN;
        state.value.theme = MR_PREFERENCES_LIGHT;
        state.values_known = 1;
        state.can_save = 0;
        return app_status;
    }

    struct config_snapshot snapshot;
    int status = read_config(app_fd, &snapshot);
    (void)close(app_fd);
    if (status == MR_PREFERENCES_VALID) {
        state.value = snapshot.value;
        state.values_known = 1;
        state.can_save = 1;
    } else if (status == MR_PREFERENCES_ABSENT) {
        state.value.language = MR_PREFERENCES_EN;
        state.value.theme = MR_PREFERENCES_LIGHT;
        state.values_known = 1;
        state.can_save = 1;
    } else {
        state.value.language = MR_PREFERENCES_EN;
        state.value.theme = MR_PREFERENCES_LIGHT;
        state.values_known = 1;
        state.can_save = 0;
    }
    return status;
}

int64_t mr_preferences_language(void) {
    return state.values_known ? state.value.language : -1;
}

int64_t mr_preferences_theme(void) {
    return state.values_known ? state.value.theme : -1;
}

int64_t mr_preferences_values_known(void) {
    return state.values_known ? 1 : 0;
}

int64_t mr_preferences_can_save(void) {
    return state.can_save ? 1 : 0;
}

int64_t mr_preferences_set_language(int64_t language) {
    return store_transaction(0, language, 0);
}

int64_t mr_preferences_set_theme(int64_t theme) {
    return store_transaction(1, theme, 0);
}

int64_t mr_preferences_reset(void) {
    return store_transaction(0, 0, 1);
}

int64_t mr_preferences_last_stage(void) {
    return last_store_stage;
}

int64_t mr_preferences_last_error(void) {
    return last_store_error;
}

int64_t mr_preferences_last_rename_succeeded(void) {
    return last_rename_succeeded;
}

int64_t mr_preferences_last_readback_status(void) {
    return last_readback_status;
}
