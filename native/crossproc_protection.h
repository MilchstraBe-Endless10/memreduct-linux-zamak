/* SPDX-License-Identifier: GPL-3.0-or-later
 * Shared, conservative process identity rules for the candidate scanner and
 * privileged cross-process helper.
 */
#ifndef MEMREDUCT_CROSSPROC_PROTECTION_H
#define MEMREDUCT_CROSSPROC_PROTECTION_H

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define MR_XPROC_COMM_TRUNCATED 15u

static const char *const mr_xproc_protected_names[] = {
    "polkitd",
    "polkit-gnome-authentication-agent-1",
    "polkit-kde-authentication-agent-1",
    "hyprpolkitagent",
    "systemd",
    "systemd-logind",
    "dbus-daemon",
    "dbus-broker",
    "dbus-broker-launch",
    "Hyprland",
    "hyprland",
    "Xorg",
    "Xwayland",
    "gnome-shell",
    "kwin_wayland",
    "kwin_x11",
    "plasmashell",
    "mutter",
    "sway",
    "wayfire",
    "pipewire",
    "pipewire-pulse",
    "wireplumber",
    "xdg-desktop-portal",
    "xdg-desktop-portal-hyprland",
    NULL
};

static inline int mr_xproc_name_is_protected(const char *name)
{
    if (name == NULL || name[0] == '\0') {
        return 0;
    }
    size_t length = strlen(name);
    for (size_t index = 0; mr_xproc_protected_names[index] != NULL; index++) {
        const char *protected = mr_xproc_protected_names[index];
        if (strcmp(name, protected) == 0) {
            return 1;
        }
        if (length == MR_XPROC_COMM_TRUNCATED &&
            strlen(protected) > MR_XPROC_COMM_TRUNCATED &&
            strncmp(name, protected, MR_XPROC_COMM_TRUNCATED) == 0) {
            return 1;
        }
    }
    return 0;
}

/* Return -1 for an unidentifiable executable, 0 for a known non-protected
 * basename, and 1 for a protected program. The supplied path must come from a
 * successful, non-truncated readlink("/proc/<pid>/exe"). */
static inline int mr_xproc_exe_is_protected(const char *path)
{
    if (path == NULL || path[0] != '/') {
        return -1;
    }
    size_t length = strlen(path);
    static const char deleted[] = " (deleted)";
    if (length > sizeof(deleted) - 1u &&
        strcmp(path + length - (sizeof(deleted) - 1u), deleted) == 0) {
        length -= sizeof(deleted) - 1u;
    }
    const char *base = strrchr(path, '/');
    base = base == NULL ? path : base + 1;
    size_t base_length = length - (size_t)(base - path);
    if (base_length == 0u || base_length >= 256u) {
        return -1;
    }
    char name[256];
    memcpy(name, base, base_length);
    name[base_length] = '\0';
    return mr_xproc_name_is_protected(name);
}

/* Return 1 only for a user's application scope with a matching UID, 0 for a
 * known protected/non-application scope, and -1 for malformed or unknown
 * cgroup layouts. Callers fail closed for both non-application and unknown. */
static inline int mr_xproc_cgroup_path_is_user_app(const char *path,
                                                    size_t path_length,
                                                    uint32_t requester_uid)
{
    if (path == NULL || path_length < 2u || path[0] != '/') {
        return -1;
    }

    char expected_user[32];
    int expected_length = snprintf(expected_user, sizeof(expected_user),
                                   "user-%u.slice", requester_uid);
    if (expected_length <= 0 || (size_t)expected_length >= sizeof(expected_user)) {
        return -1;
    }

    int saw_user_slice = 0;
    int saw_matching_user = 0;
    int saw_app_slice = 0;
    int saw_app_scope = 0;
    const char *cursor = path + 1;
    const char *end = path + path_length;
    while (cursor < end) {
        const char *component_end = memchr(cursor, '/', (size_t)(end - cursor));
        if (component_end == NULL) {
            component_end = end;
        }
        size_t component_length = (size_t)(component_end - cursor);
        if (component_length == 0u ||
            (component_length == 1u && cursor[0] == '.') ||
            (component_length == 2u && cursor[0] == '.' && cursor[1] == '.')) {
            return -1;
        }

        if ((component_length == sizeof("system.slice") - 1u &&
             memcmp(cursor, "system.slice", component_length) == 0) ||
            (component_length == sizeof("init.scope") - 1u &&
             memcmp(cursor, "init.scope", component_length) == 0) ||
            (component_length == sizeof("machine.slice") - 1u &&
             memcmp(cursor, "machine.slice", component_length) == 0)) {
            return 0;
        }
        if (component_length > sizeof("session-") - 1u + sizeof(".scope") - 1u &&
            memcmp(cursor, "session-", sizeof("session-") - 1u) == 0 &&
            memcmp(component_end - sizeof(".scope") + 1u, ".scope",
                   sizeof(".scope") - 1u) == 0) {
            return 0;
        }

        if (component_length == sizeof("user.slice") - 1u &&
            memcmp(cursor, "user.slice", component_length) == 0) {
            saw_user_slice = 1;
        } else if (saw_user_slice && component_length == (size_t)expected_length &&
                   memcmp(cursor, expected_user, component_length) == 0) {
            saw_matching_user = 1;
        } else if (saw_user_slice && saw_matching_user &&
                   component_length == sizeof("app.slice") - 1u &&
                   memcmp(cursor, "app.slice", component_length) == 0) {
            saw_app_slice = 1;
        } else if (saw_app_slice && component_length > sizeof("app-") - 1u +
                                                sizeof(".scope") - 1u &&
                   memcmp(cursor, "app-", sizeof("app-") - 1u) == 0 &&
                   memcmp(component_end - sizeof(".scope") + 1u, ".scope",
                          sizeof(".scope") - 1u) == 0 &&
                   component_end == end) {
            saw_app_scope = 1;
        }

        if (component_end == end) {
            break;
        }
        cursor = component_end + 1;
    }

    if (!saw_user_slice) {
        return -1;
    }
    return saw_matching_user && saw_app_slice && saw_app_scope ? 1 : 0;
}

/* /proc/<pid>/cgroup uses hierarchy-id:controllers:path per line. Every
 * reported hierarchy must identify the same user's app scope; one protected,
 * root, malformed, or unsupported hierarchy makes the target unusable. */
static inline int mr_xproc_cgroup_text_is_user_app(const char *text,
                                                    uint32_t requester_uid)
{
    if (text == NULL || text[0] == '\0') {
        return -1;
    }
    const char *line = text;
    int lines = 0;
    while (*line != '\0') {
        const char *line_end = strchr(line, '\n');
        if (line_end == NULL) {
            line_end = line + strlen(line);
        }
        if (line_end == line) {
            return -1;
        }
        const char *first = memchr(line, ':', (size_t)(line_end - line));
        if (first == NULL || first == line) {
            return -1;
        }
        for (const char *digit = line; digit < first; digit++) {
            if (*digit < '0' || *digit > '9') {
                return -1;
            }
        }
        const char *second = memchr(first + 1, ':', (size_t)(line_end - first - 1));
        if (second == NULL || second + 1 == line_end) {
            return -1;
        }
        int classification = mr_xproc_cgroup_path_is_user_app(
            second + 1, (size_t)(line_end - second - 1), requester_uid);
        if (classification != 1) {
            return classification;
        }
        lines++;
        if (*line_end == '\0') {
            line = line_end;
        } else {
            line = line_end + 1;
        }
    }
    return lines > 0 ? 1 : -1;
}

#endif /* MEMREDUCT_CROSSPROC_PROTECTION_H */
