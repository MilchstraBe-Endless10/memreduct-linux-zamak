/* SPDX-License-Identifier: GPL-3.0-or-later */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Stable GTK3/GLib ABI declarations keep this bridge independent of Gtk headers. */
typedef struct _GtkWidget GtkWidget;
typedef struct _GtkWindow GtkWindow;
typedef struct _GtkBin GtkBin;
typedef struct _GtkContainer GtkContainer;
typedef struct _GList GList;
struct _GList {
    void *data;
    GList *next;
    GList *prev;
};

extern GList *gtk_window_list_toplevels(void);
extern const char *gtk_window_get_title(GtkWindow *window);
extern void gtk_widget_show(GtkWidget *widget);
extern void gtk_widget_hide(GtkWidget *widget);
extern GtkWidget *gtk_bin_get_child(GtkBin *bin);
extern GList *gtk_container_get_children(GtkContainer *container);
extern unsigned int gtk_container_get_border_width(GtkContainer *container);
extern void gtk_container_set_border_width(GtkContainer *container, unsigned int border_width);
extern void gtk_widget_queue_resize(GtkWidget *widget);
extern void gtk_window_present(GtkWindow *window);
extern unsigned long gtk_widget_get_type(void);
extern unsigned int g_signal_lookup(const char *name, unsigned long type);
extern unsigned long gtk_container_get_type(void);
extern int g_type_check_instance_is_a(void *instance, unsigned long iface_type);
extern const char *g_type_name_from_instance(void *instance);
extern const char *gtk_widget_get_name(GtkWidget *widget);
extern GtkWidget *gtk_widget_get_parent(GtkWidget *widget);
extern unsigned int gtk_get_major_version(void);
extern unsigned int gtk_get_minor_version(void);
extern unsigned int gtk_get_micro_version(void);
extern unsigned long g_signal_connect_data(
    GtkWidget *instance,
    const char *detailed_signal,
    int (*callback)(GtkWidget *widget, void *event, void *data),
    void *data,
    void *destroy_data,
    int connect_flags);
extern void g_list_free(GList *list);

static int settings_close_requested;
static GtkWindow *settings_window;
static int settings_close_handler_connected;
static unsigned long settings_close_handler_id;
static int settings_geometry_prepared;
static int gtk_version_logged;

static int on_settings_delete(GtkWidget *widget, void *event, void *data)
{
    (void)widget;
    (void)event;
    (void)data;
    if (getenv("MEMREDUCT_CI") != NULL) {
        fputs("CI_NATIVE_SETTINGS_DELETE_EVENT\n", stderr);
        fflush(stderr);
    }
    settings_close_requested = 1;
    return 1; /* Keep the reusable secondary window alive; Zamak returns to main. */
}

/* Show/hide the independent settings top-level without resizing the main window. */
int64_t mr_gui_window_mode(int64_t show_settings)
{
    GList *windows = gtk_window_list_toplevels();
    GtkWindow *main_window = NULL;
    GtkWindow *settings_window_found = NULL;

    for (GList *item = windows; item != NULL; item = item->next) {
        GtkWindow *window = (GtkWindow *)item->data;
        const char *title = gtk_window_get_title(window);
        if (title == NULL) {
            continue;
        }
        if (strcmp(title, "Mem Reduct") == 0) {
            main_window = window;
        } else if (strcmp(title, "Settings - Mem Reduct") == 0) {
            settings_window_found = window;
        }
    }

    if (!gtk_version_logged && getenv("MEMREDUCT_CI") != NULL) {
        fprintf(stderr, "CI_GTK_RUNTIME version=%u.%u.%u\n",
                gtk_get_major_version(), gtk_get_minor_version(), gtk_get_micro_version());
        fflush(stderr);
        gtk_version_logged = 1;
    }

    if (settings_window_found != NULL && !settings_close_handler_connected) {
        const unsigned int delete_signal_id =
            g_signal_lookup("delete-event", gtk_widget_get_type());
        if (delete_signal_id == 0) {
            if (getenv("MEMREDUCT_CI") != NULL) {
                fputs("CI_NATIVE_SETTINGS_DELETE_HANDLER signal_unavailable=true\n", stderr);
                fflush(stderr);
            }
            g_list_free(windows);
            return 0;
        }
        settings_close_handler_id = g_signal_connect_data(
            (GtkWidget *)settings_window_found,
            "delete-event",
            on_settings_delete,
            NULL,
            NULL,
            0); /* Run after the existing libui handler, before GTK's default close. */
        if (getenv("MEMREDUCT_CI") != NULL) {
            fprintf(stderr,
                    "CI_NATIVE_SETTINGS_DELETE_HANDLER signal_id=%u phase=normal connected=%d\n",
                    delete_signal_id,
                    settings_close_handler_id != 0);
            fflush(stderr);
        }
        if (settings_close_handler_id != 0) {
            settings_window = settings_window_found;
            settings_close_handler_connected = 1;
        }
    }

    if (settings_window_found != NULL && !settings_geometry_prepared) {
        GtkWidget *root = gtk_bin_get_child((GtkBin *)settings_window_found);
        const char *root_type = root == NULL ? "(null)" : g_type_name_from_instance(root);
        GList *children = root != NULL && g_type_check_instance_is_a(root, gtk_container_get_type()) &&
                          root_type != NULL && strcmp(root_type, "GtkBox") == 0
                              ? gtk_container_get_children((GtkContainer *)root)
                              : NULL;
        unsigned int child_count = 0;
        unsigned int holder_candidates = 0;
        GtkWidget *holder = NULL;
        unsigned int child_index = 0;
        for (GList *child = children; child != NULL; child = child->next) {
            GtkWidget *candidate = (GtkWidget *)child->data;
            child_count++;
            const char *candidate_type = candidate == NULL ? NULL : g_type_name_from_instance(candidate);
            const int is_container = candidate != NULL &&
                                     g_type_check_instance_is_a(candidate, gtk_container_get_type());
            const unsigned int child_border = is_container
                                                  ? gtk_container_get_border_width((GtkContainer *)candidate)
                                                  : 0;
            if (getenv("MEMREDUCT_CI") != NULL) {
                fprintf(stderr, "CI_SETTINGS_WIDGET_CHILD index=%u type=%s is_container=%d border=%u\n",
                        child_index,
                        candidate_type != NULL ? candidate_type : "(unknown)",
                        is_container,
                        child_border);
                fflush(stderr);
            }
            if (is_container && candidate_type != NULL && strcmp(candidate_type, "GtkBox") == 0 &&
                child_border == 12) {
                holder = candidate;
                holder_candidates++;
            }
            child_index++;
        }
        const char *holder_type = holder == NULL ? "(null)" : g_type_name_from_instance(holder);
        const char *holder_name = holder == NULL ? "(null)" : gtk_widget_get_name(holder);
        if (getenv("MEMREDUCT_CI") != NULL) {
            fprintf(stderr,
                    "CI_SETTINGS_WIDGET_TREE root_type=%s children=%u holder_candidates=%u holder_type=%s holder_name=%s\n",
                    root_type != NULL ? root_type : "(unknown)",
                    child_count,
                    holder_candidates,
                    holder_type != NULL ? holder_type : "(unknown)",
                    holder_name != NULL ? holder_name : "(unknown)");
            fflush(stderr);
        }
        if (gtk_get_major_version() == 3 && root != NULL && root_type != NULL && strcmp(root_type, "GtkBox") == 0 &&
            holder_candidates == 1 && holder != NULL && gtk_widget_get_parent(holder) == root) {
            const unsigned int old_border = gtk_container_get_border_width((GtkContainer *)holder);
            if (old_border == 12) {
                /* The unique direct GtkBox child with the tested 12 px inset is the libui-ng content holder. */
                gtk_container_set_border_width((GtkContainer *)holder, 0);
                gtk_widget_queue_resize(holder);
                gtk_widget_queue_resize(root);
                gtk_widget_queue_resize((GtkWidget *)settings_window_found);
                settings_geometry_prepared = 1;
                if (getenv("MEMREDUCT_CI") != NULL) {
                    fputs("CI_SETTINGS_CONTENT_BORDER old=12 new=0 verified=true\n", stderr);
                    fflush(stderr);
                }
            } else if (getenv("MEMREDUCT_CI") != NULL) {
                fprintf(stderr, "CI_SETTINGS_CONTENT_BORDER unsupported_border=%u expected=12_or_0\n", old_border);
                fflush(stderr);
            }
        } else if (getenv("MEMREDUCT_CI") != NULL) {
            fprintf(stderr,
                    "CI_SETTINGS_WIDGET_TREE unsupported=true gtk_major=%u root_type=%s holder_candidates=%u holder_type=%s\n",
                    gtk_get_major_version(),
                    root_type != NULL ? root_type : "(unknown)",
                    holder_candidates,
                    holder_type != NULL ? holder_type : "(unknown)");
            fflush(stderr);
        }
        g_list_free(children);
    }

    if (settings_window_found == NULL || main_window == NULL || !settings_geometry_prepared ||
        !settings_close_handler_connected) {
        g_list_free(windows);
        return 0;
    }

    if (show_settings != 0) {
        gtk_widget_show((GtkWidget *)settings_window_found);
        gtk_window_present(settings_window_found);
    } else {
        gtk_widget_hide((GtkWidget *)settings_window_found);
        gtk_window_present(main_window);
    }
    g_list_free(windows);
    return 1;
}

int64_t mr_gui_take_settings_close_request(void)
{
    const int requested = settings_close_requested;
    settings_close_requested = 0;
    return requested;
}

int64_t mr_gui_settings_window_exists(void)
{
    return settings_window != NULL;
}

int64_t mr_gui_settings_close_handler_connected(void)
{
    return settings_close_handler_id != 0;
}
