/* SPDX-License-Identifier: GPL-3.0-or-later */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Stable GTK3/GLib ABI declarations keep this bridge independent of Gtk headers. */
typedef struct _GtkWidget GtkWidget;
typedef struct _GtkWindow GtkWindow;
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
extern void gtk_window_present(GtkWindow *window);
extern unsigned long gtk_widget_get_type(void);
extern unsigned int g_signal_lookup(const char *name, unsigned long type);
extern unsigned int g_signal_handlers_disconnect_matched(
    void *instance,
    unsigned int mask,
    unsigned int signal_id,
    unsigned long detail,
    void *closure,
    void *func,
    void *data);
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

    if (settings_window_found != NULL && !settings_close_handler_connected) {
        const unsigned int delete_signal_id =
            g_signal_lookup("delete-event", gtk_widget_get_type());
        unsigned int disconnected = 0;

        if (delete_signal_id != 0) {
            /* Replace prior close handlers so this reusable window can report close to Zamak. */
            disconnected = g_signal_handlers_disconnect_matched(
                (GtkWidget *)settings_window_found,
                1, /* G_SIGNAL_MATCH_ID */
                delete_signal_id,
                0,
                NULL,
                NULL,
                NULL);
        }
        settings_close_handler_id = g_signal_connect_data(
            (GtkWidget *)settings_window_found,
            "delete-event",
            on_settings_delete,
            NULL,
            NULL,
            0);
        if (getenv("MEMREDUCT_CI") != NULL) {
            fprintf(stderr,
                    "CI_NATIVE_SETTINGS_DELETE_HANDLER signal_id=%u disconnected=%u connected=%d\n",
                    delete_signal_id,
                    disconnected,
                    settings_close_handler_id != 0);
            fflush(stderr);
        }
        if (settings_close_handler_id != 0) {
            settings_window = settings_window_found;
            settings_close_handler_connected = 1;
        }
    }

    if (settings_window_found == NULL || main_window == NULL ||
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
