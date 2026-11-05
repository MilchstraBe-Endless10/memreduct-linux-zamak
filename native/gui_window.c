/* SPDX-License-Identifier: GPL-3.0-or-later */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef MR_GUI_PRODUCTION
/* This translation unit is shared with CI. Production builds include it through
 * gui_window_prod.c, whose inert macros erase environment names and diagnostic
 * arguments before compilation. In particular, CI_CONTROLS cannot trigger
 * compositor operations in the shipped binary. */
#define getenv(...) NULL
#define fprintf(...) ((void)0)
#define fputs(...) ((void)0)
#define fflush(...) ((void)0)
#endif

/* Stable GTK3/GLib ABI declarations keep this bridge independent of Gtk headers. */
typedef struct _GtkWidget GtkWidget;
typedef struct _GtkHeaderBar GtkHeaderBar;
typedef struct _GtkWindow GtkWindow;
typedef struct _GtkSettings GtkSettings;
typedef struct _GtkBin GtkBin;
typedef struct _GtkContainer GtkContainer;
typedef struct _GdkWindow GdkWindow;
typedef struct _GList GList;
typedef struct {
    double red;
    double green;
    double blue;
    double alpha;
} GdkRGBA;
struct _GList {
    void *data;
    GList *next;
    GList *prev;
};

extern GList *gtk_window_list_toplevels(void);
extern GtkSettings *gtk_settings_get_default(void);
extern void gtk_settings_set_string_property(
    GtkSettings *settings,
    const char *name,
    const char *value,
    const char *origin);
extern const char *gtk_window_get_title(GtkWindow *window);
extern void gtk_window_set_default_size(GtkWindow *window, int width, int height);
/* Exact client-size pinning. Declared structurally to keep this bridge free of
 * Gtk headers, exactly like every other ABI above. */
typedef struct {
    int min_width, min_height, max_width, max_height;
    int base_width, base_height, width_inc, height_inc;
    double min_aspect, max_aspect;
    int gravity;
} MrGdkGeometry;
#define MR_GDK_HINT_MIN_SIZE (1 << 1)
#define MR_GDK_HINT_MAX_SIZE (1 << 2)
#define MR_GDK_HINT_BASE_SIZE (1 << 4)
#define MR_GDK_GRAVITY_NORTH_WEST 1
extern void gtk_window_set_geometry_hints(GtkWindow *window, GtkWidget *geometry_widget,
                                          MrGdkGeometry *geometry, int geom_flags);
extern void gtk_window_resize(GtkWindow *window, int width, int height);
extern void gtk_window_set_resizable(GtkWindow *window, int resizable);
extern int gtk_window_get_resizable(GtkWindow *window);
extern void gtk_widget_show(GtkWidget *widget);
extern void gtk_widget_hide(GtkWidget *widget);
extern void gtk_widget_realize(GtkWidget *widget);
extern void gtk_widget_unrealize(GtkWidget *widget);
extern GtkWidget *gtk_bin_get_child(GtkBin *bin);
extern GList *gtk_container_get_children(GtkContainer *container);
extern unsigned int gtk_container_get_border_width(GtkContainer *container);
extern void gtk_container_set_border_width(GtkContainer *container, unsigned int border_width);
extern void gtk_widget_queue_resize(GtkWidget *widget);
extern unsigned int g_idle_add(int (*callback)(void *data), void *data);
extern void gtk_window_get_size(GtkWindow *window, int *width, int *height);
extern int gtk_window_get_decorated(GtkWindow *window);
extern GtkWidget *gtk_window_get_titlebar(GtkWindow *window);
extern GtkWidget *gtk_header_bar_new(void);
extern void gtk_header_bar_set_title(GtkHeaderBar *bar, const char *title);
extern void gtk_header_bar_set_subtitle(GtkHeaderBar *bar, const char *subtitle);
extern void gtk_header_bar_set_show_close_button(GtkHeaderBar *bar, int setting);
extern void gtk_header_bar_set_decoration_layout(GtkHeaderBar *bar, const char *layout);
extern void gtk_window_set_titlebar(GtkWindow *window, GtkWidget *titlebar);
extern void gtk_widget_show_all(GtkWidget *widget);
extern int gtk_widget_get_realized(GtkWidget *widget);
extern int gtk_widget_get_mapped(GtkWidget *widget);
extern int gtk_widget_get_allocated_width(GtkWidget *widget);
extern int gtk_widget_get_allocated_height(GtkWidget *widget);
extern void gtk_widget_queue_draw(GtkWidget *widget);
extern void gtk_widget_add_events(GtkWidget *widget, int events);
extern GdkWindow *gtk_widget_get_window(GtkWidget *widget);
extern void gtk_widget_override_background_color(GtkWidget *widget, unsigned int state, const GdkRGBA *color);
extern void gtk_widget_set_margin_start(GtkWidget *widget, int margin);
extern void gtk_widget_set_margin_end(GtkWidget *widget, int margin);
extern void gtk_widget_set_margin_top(GtkWidget *widget, int margin);
extern void gtk_widget_set_margin_bottom(GtkWidget *widget, int margin);
extern void gtk_window_present(GtkWindow *window);
void gtk_window_iconify(GtkWindow *window);
void gtk_window_deiconify(GtkWindow *window);
void gtk_window_maximize(GtkWindow *window);
void gtk_window_unmaximize(GtkWindow *window);
int gtk_window_is_maximized(GtkWindow *window);

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
extern unsigned long g_signal_handler_find(
    void *instance,
    unsigned int mask,
    unsigned int signal_id,
    unsigned int detail,
    void *closure,
    void *func,
    void *data);
extern void g_signal_handler_block(void *instance, unsigned long handler_id);
extern void g_signal_handler_unblock(void *instance, unsigned long handler_id);
extern unsigned long g_signal_connect_data(
    GtkWidget *instance,
    const char *detailed_signal,
    int (*callback)(GtkWidget *widget, void *event, void *data),
    void *data,
    void *destroy_data,
    int connect_flags);
extern void g_list_free(GList *list);
extern int gdk_event_get_root_coords(const void *event, double *x_root, double *y_root);
extern int gdk_event_get_scroll_direction(const void *event, int *direction);
extern int gdk_event_get_scroll_deltas(const void *event, double *delta_x, double *delta_y);
extern int gdk_window_get_origin(GdkWindow *window, int *x, int *y);
extern void gtk_widget_get_preferred_width(GtkWidget *widget, int *minimum, int *natural);
extern void gtk_widget_get_preferred_height(GtkWidget *widget, int *minimum, int *natural);
extern void gtk_widget_get_size_request(GtkWidget *widget, int *width, int *height);
extern void gtk_widget_set_size_request(GtkWidget *widget, int width, int height);
extern void gtk_widget_set_hexpand(GtkWidget *widget, int expand);
extern void gtk_widget_set_vexpand(GtkWidget *widget, int expand);
extern void gtk_window_get_default_size(GtkWindow *window, int *width, int *height);
extern void *gdk_display_get_default(void);
extern unsigned int g_timeout_add(unsigned int interval, int (*callback)(void *data), void *data);

static int settings_close_requested;
static GtkWindow *settings_window;
static int settings_close_handler_connected;
static unsigned long settings_close_handler_id;
static unsigned long settings_preexisting_close_handler_id;
static unsigned int settings_delete_signal_id;
static int settings_geometry_prepared;
static int main_geometry_prepared;
static int gtk_version_logged;
static unsigned long settings_scroll_handler_id;
static int64_t settings_scroll_taken_x;
static int64_t settings_scroll_taken_y;
static double settings_scroll_smooth_remainder;

enum {
    GDK_SCROLL_UP = 0,
    GDK_SCROLL_DOWN = 1,
    GDK_SCROLL_LEFT = 2,
    GDK_SCROLL_RIGHT = 3,
    GDK_SCROLL_SMOOTH = 4,
    GDK_SCROLL_MASK = 1 << 21
};

#define SETTINGS_SCROLL_QUEUE_LIMIT 32

typedef struct {
    int64_t delta;
    int64_t x;
    int64_t y;
} SettingsScrollEvent;

static SettingsScrollEvent settings_scroll_queue[SETTINGS_SCROLL_QUEUE_LIMIT];
static unsigned int settings_scroll_head;
static unsigned int settings_scroll_count;

static int on_settings_scroll(GtkWidget *widget, void *event, void *data)
{
    (void)data;
    double x_root = 0.0;
    double y_root = 0.0;
    int window_x = 0;
    int window_y = 0;
    int direction = -1;
    int64_t delta = 0;

    GdkWindow *window = gtk_widget_get_window(widget);
    if (window == NULL || !gdk_event_get_root_coords(event, &x_root, &y_root) ||
        !gdk_window_get_origin(window, &window_x, &window_y) ||
        !gdk_event_get_scroll_direction(event, &direction)) {
        return 0;
    }
    const double x = x_root - (double)window_x;
    const double y = y_root - (double)window_y;

    if (direction == GDK_SCROLL_UP) {
        delta = -1;
    } else if (direction == GDK_SCROLL_DOWN) {
        delta = 1;
    } else if (direction == GDK_SCROLL_SMOOTH) {
        double delta_x = 0.0;
        double delta_y = 0.0;
        if (!gdk_event_get_scroll_deltas(event, &delta_x, &delta_y) || delta_y == 0.0) {
            return 0;
        }
        settings_scroll_smooth_remainder += delta_y;
        if (settings_scroll_smooth_remainder >= 1.0) {
            delta = (int64_t)settings_scroll_smooth_remainder;
        } else if (settings_scroll_smooth_remainder <= -1.0) {
            delta = (int64_t)settings_scroll_smooth_remainder;
        }
        settings_scroll_smooth_remainder -= (double)delta;
    } else {
        return 0;
    }

    if (delta == 0) {
        return 0;
    }
    if (settings_scroll_count == SETTINGS_SCROLL_QUEUE_LIMIT) {
        if (getenv("MEMREDUCT_CI") != NULL) {
            fputs("CI_SETTINGS_SCROLL_QUEUE full=true\n", stderr);
            fflush(stderr);
        }
        return 0;
    }

    const unsigned int tail = (settings_scroll_head + settings_scroll_count) % SETTINGS_SCROLL_QUEUE_LIMIT;
    settings_scroll_queue[tail] = (SettingsScrollEvent){delta, (int64_t)x, (int64_t)y};
    settings_scroll_count++;
    if (getenv("MEMREDUCT_CI") != NULL) {
        fprintf(stderr, "CI_SETTINGS_SCROLL_CAPTURE x=%lld y=%lld delta=%lld queued=%u\n",
                (long long)x, (long long)y, (long long)delta, settings_scroll_count);
        fflush(stderr);
    }
    return 0;
}

static int install_settings_scroll_handler(GtkWindow *window)
{
    GtkWidget *widget = (GtkWidget *)window;
    gtk_widget_add_events(widget, GDK_SCROLL_MASK);
    const unsigned long handler_id = g_signal_connect_data(
        widget, "scroll-event", on_settings_scroll, NULL, NULL, 0);
    if (handler_id == 0) {
        return 0;
    }

    settings_scroll_handler_id = handler_id;
    if (getenv("MEMREDUCT_CI") != NULL) {
        fprintf(stderr, "CI_SETTINGS_SCROLL_HANDLER widget=%s mask=%d connected=%d coordinates=root-to-client\n",
                g_type_name_from_instance(widget), GDK_SCROLL_MASK, handler_id != 0);
        fflush(stderr);
    }
    return 1;
}

int64_t mr_gui_set_main_background(int64_t dark)
{
    GList *windows = gtk_window_list_toplevels();
    GtkWindow *main_window = NULL;
    GdkRGBA color = dark != 0
                        ? (GdkRGBA){36.0 / 255.0, 36.0 / 255.0, 36.0 / 255.0, 1.0}
                        : (GdkRGBA){1.0, 1.0, 1.0, 1.0};

    for (GList *item = windows; item != NULL; item = item->next) {
        GtkWindow *window = (GtkWindow *)item->data;
        const char *title = gtk_window_get_title(window);
        if (title != NULL && strcmp(title, "Mem Reduct") == 0) {
            main_window = window;
            break;
        }
    }

    if (main_window != NULL) {
        GtkWidget *root = gtk_bin_get_child((GtkBin *)main_window);
        gtk_widget_override_background_color((GtkWidget *)main_window, 0, &color);
        if (root != NULL) {
            gtk_widget_override_background_color(root, 0, &color);
            if (g_type_check_instance_is_a(root, gtk_container_get_type())) {
                GList *children = gtk_container_get_children((GtkContainer *)root);
                for (GList *child = children; child != NULL; child = child->next) {
                    GtkWidget *widget = (GtkWidget *)child->data;
                    gtk_widget_override_background_color(widget, 0, &color);
                }
                g_list_free(children);
            }
        }
        gtk_widget_queue_draw((GtkWidget *)main_window);
    }

    if (getenv("MEMREDUCT_CI") != NULL) {
        fprintf(stderr, "CI_MAIN_BACKGROUND mode=%s applied=%d\n", dark != 0 ? "dark" : "light",
                main_window != NULL);
        fflush(stderr);
    }
    g_list_free(windows);
    return main_window != NULL ? 1 : 0;
}

int64_t mr_gui_set_compat_font(void)
{
    const char *candidate = getenv("P2C_FONT_CANDIDATE");

    /* A font override is an isolated experiment, never a production default. */
    if (getenv("MEMREDUCT_CI") == NULL) {
        return 2;
    }
    if (candidate == NULL || strcmp(candidate, "default") == 0) {
        return 2;
    }
    if (strcmp(candidate, "liberation") != 0) {
        return 0;
    }

    GtkSettings *settings = gtk_settings_get_default();
    if (settings == NULL) {
        return 0;
    }

    /* This override is used only by the isolated English/CJK comparison lane. */
    gtk_settings_set_string_property(settings, "gtk-font-name", "Liberation Sans 8",
                                     "Mem Reduct v3.5.2 resource font compatibility");
    return 1;
}

/* Keep the main canvas at the client top edge while preserving its source x inset. */
#ifndef MR_GUI_PRODUCTION
/* One-shot structural dump of a toplevel, gated on MEMREDUCT_CI. The probe that
 * worked standalone did not work here, and guessing at the tree twice already
 * cost a rebuild each time; this prints what is actually there. */
/* Requirement for issue #11: report the real GtkWidget hierarchy of the production
 * window instead of guessing child indices. Every node prints its class, the pointer
 * identity of the holder/canvas, the GTK size request (preferred min/natural) next to
 * the actual allocation, its resize policy and whether it is realized/mapped. */
static void dump_window_node(GtkWidget *widget, const char *tag, unsigned int depth,
                             unsigned int index, unsigned int parent_index,
                             unsigned int max_depth)
{
    if (widget == NULL) {
        return;
    }
    const char *type = g_type_name_from_instance(widget);
    int req_min_w = -1, req_nat_w = -1, req_min_h = -1, req_nat_h = -1;
    int explicit_w = -1, explicit_h = -1;
    gtk_widget_get_preferred_width(widget, &req_min_w, &req_nat_w);
    gtk_widget_get_preferred_height(widget, &req_min_h, &req_nat_h);
    gtk_widget_get_size_request(widget, &explicit_w, &explicit_h);
    const int is_container = g_type_check_instance_is_a(widget, gtk_container_get_type()) != 0;
    unsigned int border = is_container ? gtk_container_get_border_width((GtkContainer *)widget) : 0;
    const char *name = gtk_widget_get_name(widget);
    GtkWidget *parent = gtk_widget_get_parent(widget);
    const char *parent_type = parent == NULL ? NULL : g_type_name_from_instance(parent);
    fprintf(stderr,
            "CI_TREE_DEPTH tag=%s depth=%u index=%u parent=%u type=%s name=%s ptr=%p "
            "alloc=%dx%d requested=%dx%d preferred=%dx%d..%dx%d border=%u "
            "realized=%d mapped=%d responsive=%d is_canvas=%d parent_type=%s\n",
            tag, depth, index, parent_index, type != NULL ? type : "(null)",
            name != NULL ? name : "(null)", (void *)widget,
            gtk_widget_get_allocated_width(widget), gtk_widget_get_allocated_height(widget),
            explicit_w, explicit_h, req_min_w, req_min_h, req_nat_w, req_nat_h,
            border, gtk_widget_get_realized(widget), gtk_widget_get_mapped(widget),
            gtk_widget_get_mapped(widget) && gtk_widget_get_realized(widget),
            type != NULL && (strcmp(type, "GtkDrawingArea") == 0 ||
                             (name != NULL && strcmp(name, "areaWidget") == 0)),
            parent_type != NULL ? parent_type : "(null)");
    if (!is_container || depth >= max_depth) {
        return;
    }
    GList *children = gtk_container_get_children((GtkContainer *)widget);
    unsigned int child_index = 0;
    for (GList *item = children; item != NULL; item = item->next) {
        dump_window_node((GtkWidget *)item->data, tag, depth + 1, child_index, index, max_depth);
        child_index++;
    }
    g_list_free(children);
}

static void dump_window_tree(GtkWindow *window, const char *tag)
{
    if (window == NULL || getenv("MEMREDUCT_CI") == NULL) {
        return;
    }
    int client_w = -1, client_h = -1;
    int default_w = -1, default_h = -1;
    gtk_window_get_size(window, &client_w, &client_h);
    gtk_window_get_default_size(window, &default_w, &default_h);
    void *display = gdk_display_get_default();
    const char *backend = display == NULL ? "(none)" : g_type_name_from_instance(display);
    GtkWidget *titlebar = gtk_window_get_titlebar(window);
    const char *titlebar_type = titlebar == NULL ? "(none)" : g_type_name_from_instance(titlebar);
    /* gtk_window_get_size() returns the CLIENT area (title bar excluded). The toplevel
     * allocation is that plus the measured header bar; both are printed, never conflated. */
    int titlebar_h = titlebar == NULL ? 0 : gtk_widget_get_allocated_height(titlebar);
    GtkWidget *root = gtk_bin_get_child((GtkBin *)window);
    fprintf(stderr,
            "CI_TREE tag=%s window=%p title=\"%s\" size=%dx%d default=%dx%d "
            "client=%dx%d titlebar_h=%d toplevel_expect=%dx%d backend=%s decorated=%d resizable=%d "
            "realized=%d mapped=%d has_titlebar=%d titlebar_type=%s\n",
            tag, (void *)window, gtk_window_get_title(window), client_w, client_h,
            default_w, default_h, client_w, client_h, titlebar_h,
            client_w, client_h >= 0 ? client_h + titlebar_h : -1, backend, gtk_window_get_decorated(window),
            gtk_window_get_resizable(window), gtk_widget_get_realized((GtkWidget *)window),
            gtk_widget_get_mapped((GtkWidget *)window), titlebar != NULL, titlebar_type);
    if (root == NULL) {
        fprintf(stderr, "CI_TREE_DEPTH tag=%s depth=0 root=none\n", tag);
        fflush(stderr);
        return;
    }
    dump_window_node(root, tag, 0, 0, 0, 6);
    fflush(stderr);
}
#else
static void dump_window_tree(GtkWindow *window, const char *tag)
{
    (void)window;
    (void)tag;
}
#endif

/* Wayland compositors draw no server-side decorations, so a GTK3 toplevel has no
 * title bar unless the client supplies one. The Windows original is an ordinary
 * captioned dialog that merely lacks a resize frame, so this build draws the
 * caption to keep minimise, maximise and close reachable.
 *
 * The tree dump showed mapped=0 when this file first touches the window, which is
 * why attaching a header bar directly was overridden: the size set before map is
 * replaced by the map pass. The bar is therefore attached on the first map event,
 * once and only once. */
struct mr_decoration_state {
    const char *title;
    const char *subtitle;
    const char *layout;
    int applied;
};

/* All three buttons, now that the pinned geometry hints keep the normal size at
 * 337x369 while leaving the toplevel resizable -- which is what makes maximize
 * actually do something instead of being a visible-but-dead control. */
static struct mr_decoration_state main_decoration = {"Mem Reduct", NULL,
                                                     "appmenu:minimize,maximize,close", 0};
static GtkWindow *main_window_ref = NULL;
static GtkWindow *settings_window_ref = NULL;
/* The settings dialog is modal in the original and has no maximize affordance;
 * asking for one would surface a visible-but-disabled button (Issue #11). */
static struct mr_decoration_state settings_decoration = {"Settings - Mem Reduct", "Mem Reduct",
                                                         "appmenu:minimize,close", 0};

static void apply_decoration_layout(void)
{
    GtkSettings *settings = gtk_settings_get_default();
    if (settings == NULL) {
        return;
    }
    /* The desktop default here is "appmenu:close", which removes minimise and
     * maximise from every client-side decoration. Override it for this process
     * only; the user's global preference stays untouched. */
    gtk_settings_set_string_property(settings, "gtk-decoration-layout",
                                     "appmenu:minimize,maximize,close", "Mem Reduct");
}

static int attach_header_bar(GtkWindow *window, struct mr_decoration_state *state)
{
    if (window == NULL || state == NULL || state->applied) {
        return 0;
    }
    if (gtk_window_get_titlebar(window) != NULL) {
        state->applied = 1;
        return 0;
    }
    GtkWidget *widget = (GtkWidget *)window;
    const int was_realized = gtk_widget_get_realized(widget);
    if (was_realized) {
        /* Zamak std/ui realizes its GtkWindow while constructing it. GTK3
         * requires a custom titlebar to be attached before realization. The
         * top-level is still unmapped here, so unrealize/attach/realize keeps
         * the client window identity while letting GTK negotiate the bar at
         * the correct lifecycle stage. Never mutate a mapped window this way. */
        if (gtk_widget_get_mapped(widget)) {
            return 0;
        }
        gtk_widget_unrealize(widget);
    }
    GtkWidget *bar = gtk_header_bar_new();
    gtk_header_bar_set_title((GtkHeaderBar *)bar, state->title);
    gtk_header_bar_set_show_close_button((GtkHeaderBar *)bar, 1);
    /* show_close_button alone yields only a close button; this layout is what
     * actually asks for all three. */
    gtk_header_bar_set_decoration_layout((GtkHeaderBar *)bar,
                                         state->layout != NULL ? state->layout
                                                               : "appmenu:minimize,maximize,close");
    if (state->subtitle != NULL) {
        gtk_header_bar_set_subtitle((GtkHeaderBar *)bar, state->subtitle);
    }
    gtk_window_set_titlebar(window, bar);
    gtk_widget_show_all(bar);
    if (was_realized) {
        gtk_widget_realize(widget);
    }
    state->applied = 1;
    if (getenv("MEMREDUCT_CI") != NULL) {
        fprintf(stderr, "CI_TITLEBAR attached=true title=%s layout=%s\n", state->title,
                state->layout != NULL ? state->layout : "default");
        fflush(stderr);
    }
    return 1;
}

/* Runs once from the main loop. The map signal did not fire for these toplevels,
 * so this defers the attach to the first idle turn after the loop starts: by then
 * GTK has mapped the windows and a header bar accepted here is not overwritten. */
/*
 * Content-size contract (Issue #11).
 *
 * Once a GtkHeaderBar is attached, GtkWindow's size is the WHOLE window: the
 * header bar height is subtracted from the content area. The v3.5.2 IDD_MAIN
 * baseline of 337x369 describes the CLIENT area, so the toplevel has to be
 * grown by exactly the measured header bar height instead of letting the header
 * bar eat into the canvas (which clipped 34px of the drawing area).
 */
/* Pin the CLIENT size with min==max hints and no set_default_size(). Measured:
 * calling this before the toplevel is mapped is what keeps a resizable window
 * at the requested size; applying it only from the idle callback is too late,
 * because by then a tiling compositor has already claimed the window (941x983).
 * main_content_width/height are declared below, so this lives next to them. */
static int main_content_width = 0;
static int main_content_height = 0;
static int settings_content_width = 0;
static int settings_content_height = 0;

static int apply_content_size_contract(GtkWindow *window, int content_width,
                                       int content_height, const char *tag)
{
    (void)tag;
    if (window == NULL || content_width <= 0 || content_height <= 0) {
        return 0;
    }
    GtkWidget *bar = gtk_window_get_titlebar(window);
    if (bar == NULL) {
        return 0;
    }
    int bar_height = gtk_widget_get_allocated_height(bar);
    if (bar_height <= 1) {
        return 0; /* Header bar not allocated yet; a later tick retries. */
    }
    /* Issue #11 root cause, isolated with a single-variable GTK3 repro of this
     * exact widget nesting:
     *
     *   gtk_window_resize() alone            -> the canvas size (325x357)
     *   gtk_window_set_default_size() alone  -> request + 52px on both axes
     *   both                                 -> request + 52px
     *   min==max geometry hints + resize     -> exactly the request
     *
     * So the +52 was never a border constant and never the canvas minimum --
     * gtk_widget_get_preferred_width() on every layer reports 325. It comes
     * from gtk_window_set_default_size(), which GTK3 treats as the TOTAL size
     * and then adds the CSD surface on top of.
     *
     * That is why set_default_size() is never called. The size is pinned with
     * min==max geometry hints instead, which also keeps the window resizable so
     * the header bar's maximize button is genuinely live.
     *
     * Once a GtkHeaderBar is attached, gtk_window_resize() addresses the CLIENT
     * area and the header bar comes out of it -- measured 337x322 when asking
     * for 337x369. The hint therefore carries content + header bar height so
     * the client area lands on exactly content_height.
     */
    /* The size was pinned before the window was mapped (see
     * mr_gui_lock_window_sizes). Re-applying hints here fights the compositor:
     * measured gtk reporting 337x322 while hyprctl still reported 941x1030.
     * So this pass only observes and reports; it never re-pins. */
    {
        int reached_w = 0;
        int reached_h = 0;
        gtk_window_get_size(window, &reached_w, &reached_h);
        if (getenv("MEMREDUCT_CI") != NULL) {
            fprintf(stderr,
                    "CI_CONTENT_SIZE_APPLIED tag=%s content=%dx%d bar_h=%d "
                    "expected=%dx%d reached=%dx%d delta=%+dx%+d resizable=%d\n",
                    tag, content_width, content_height, bar_height,
                    content_width, content_height,
                    reached_w, reached_h, reached_w - content_width,
                    reached_h - content_height, gtk_window_get_resizable(window));
            fflush(stderr);
        }
    }
    return 1;
}

static int on_content_size_settle(void *data)
{
    (void)data;
    apply_content_size_contract(main_window_ref, main_content_width,
                                main_content_height, "main");
    apply_content_size_contract(settings_window_ref, settings_content_width,
                                settings_content_height, "settings");
    dump_window_tree(main_window_ref, "main_content_fixed");
    dump_window_tree(settings_window_ref, "settings_content_fixed");
    return 0;
}

static int on_settle_dump(void *data)
{
    (void)data;
    if (getenv("MEMREDUCT_CI") != NULL) {
        dump_window_tree(main_window_ref, "main_settled");
        dump_window_tree(settings_window_ref, "settings_settled");
    }
    return 0;
}

/*
 * Issue #11 requirement 3: the header bar buttons must reach real window-manager
 * operations. GtkHeaderBar's minimize/maximize buttons only emit gtk-window-iconify /
 * gtk-window-toggle-maximized, which need a resizable window and a live WM. This probe
 * drives the same GTK entry points the buttons use, so the CI log proves the plumbing
 * works instead of only proving that a button is painted.
 */
#ifndef MR_GUI_PRODUCTION
struct mr_control_step {
    int step;
    GtkWindow *window;
};

static int on_control_step(void *data)
{
    struct mr_control_step *state = (struct mr_control_step *)data;
    if (state == NULL || state->window == NULL) {
        return 0;
    }
    int w = -1, h = -1;
    gtk_window_get_size(state->window, &w, &h);
    switch (state->step) {
    case 0:
        gtk_window_iconify(state->window);
        fprintf(stderr, "CI_WINDOW_CONTROL step=iconify requested=true client=%dx%d\n", w, h);
        break;
    case 1:
        fprintf(stderr, "CI_WINDOW_CONTROL step=iconified mapped=%d\n",
                gtk_widget_get_mapped((GtkWidget *)state->window));
        gtk_window_deiconify(state->window);
        break;
    case 2:
        fprintf(stderr, "CI_WINDOW_CONTROL step=deiconified mapped=%d\n",
                gtk_widget_get_mapped((GtkWidget *)state->window));
        gtk_window_maximize(state->window);
        break;
    case 3:
        gtk_window_get_size(state->window, &w, &h);
        fprintf(stderr, "CI_WINDOW_CONTROL step=maximize is_maximized=%d client=%dx%d\n",
                gtk_window_is_maximized(state->window), w, h);
        gtk_window_unmaximize(state->window);
        break;
    case 4:
        gtk_window_get_size(state->window, &w, &h);
        fprintf(stderr, "CI_WINDOW_CONTROL step=unmaximize is_maximized=%d client=%dx%d\n",
                gtk_window_is_maximized(state->window), w, h);
        gtk_window_present(state->window);
        break;
    default:
        gtk_window_get_size(state->window, &w, &h);
        fprintf(stderr, "CI_WINDOW_CONTROL step=done is_maximized=%d client=%dx%d resizable=%d\n",
                gtk_window_is_maximized(state->window), w, h,
                gtk_window_get_resizable(state->window));
        free(state);
        return 0;
    }
    fflush(stderr);
    state->step++;
    return 1;
}

static void schedule_control_probe(GtkWindow *window)
{
    if (window == NULL || getenv("MEMREDUCT_CI_CONTROLS") == NULL) {
        return;
    }
    struct mr_control_step *state = calloc(1, sizeof(*state));
    if (state == NULL) {
        return;
    }
    state->window = window;
    g_timeout_add(1200, on_control_step, state);
}
#else
static void schedule_control_probe(GtkWindow *window)
{
    (void)window;
}
#endif

static int on_first_idle(void *data)
{
    (void)data;
    attach_header_bar(main_window_ref, &main_decoration);
    attach_header_bar(settings_window_ref, &settings_decoration);
    if (getenv("MEMREDUCT_CI") != NULL) {
        fprintf(stderr, "CI_TITLEBAR idle_run=true main_titlebar=%d settings_titlebar=%d\n",
                main_window_ref != NULL && gtk_window_get_titlebar(main_window_ref) != NULL,
                settings_window_ref != NULL && gtk_window_get_titlebar(settings_window_ref) != NULL);
        fflush(stderr);
    }
    /* Re-measure once the compositor/WM has allocated the decorated window: the
     * first dump happens before map, when the title bar does not exist yet. */
    dump_window_tree(main_window_ref, "main_post_titlebar");
    dump_window_tree(settings_window_ref, "settings_post_titlebar");
    g_timeout_add(400, on_content_size_settle, NULL);
    g_timeout_add(700, on_content_size_settle, NULL);
    g_timeout_add(1500, on_content_size_settle, NULL);
    g_timeout_add(2200, on_settle_dump, NULL);
    schedule_control_probe(main_window_ref);
    return 0;
}

static int prepare_main_geometry(GtkWindow *window)
{
    GtkWidget *root = gtk_bin_get_child((GtkBin *)window);
    const char *root_type = root == NULL ? NULL : g_type_name_from_instance(root);
    GList *children = root != NULL && g_type_check_instance_is_a(root, gtk_container_get_type()) &&
                      root_type != NULL && strcmp(root_type, "GtkBox") == 0
                          ? gtk_container_get_children((GtkContainer *)root)
                          : NULL;
    GtkWidget *holder = NULL;
    unsigned int candidates = 0;

    for (GList *child = children; child != NULL; child = child->next) {
        GtkWidget *candidate = (GtkWidget *)child->data;
        const int is_container = candidate != NULL &&
                                 g_type_check_instance_is_a(candidate, gtk_container_get_type());
        const char *candidate_type = candidate == NULL ? NULL : g_type_name_from_instance(candidate);
        if (is_container && candidate_type != NULL && strcmp(candidate_type, "GtkBox") == 0 &&
            gtk_container_get_border_width((GtkContainer *)candidate) == 12) {
            holder = candidate;
            candidates++;
        }
    }

    if (gtk_get_major_version() == 3 && candidates == 1 && holder != NULL &&
        gtk_widget_get_parent(holder) == root) {
        /* The 337x369 the contract asks for is the CLIENT area Zamak already
         * created. Re-adding a 12px start and bottom margin on top of it grew
         * the negotiated window to 389x422 (measured), which pushed the canvas
         * to 377x410 and clipped nothing but did break the reference size.
         * Clear every inset instead of swapping one inset for another. */
        gtk_container_set_border_width((GtkContainer *)holder, 0);
        gtk_widget_set_margin_start(holder, 0);
        gtk_widget_set_margin_end(holder, 0);
        gtk_widget_set_margin_top(holder, 0);
        gtk_widget_set_margin_bottom(holder, 0);
        gtk_widget_queue_resize(holder);
        gtk_widget_queue_resize(root);
        gtk_widget_queue_resize((GtkWidget *)window);
        main_geometry_prepared = 1;
        if (getenv("MEMREDUCT_CI") != NULL) {
            fputs("CI_MAIN_CONTENT_INSET old=12 new=0 margins=start:0,end:0,top:0,bottom:0 verified=true\n", stderr);
            fflush(stderr);
        }
    } else if (getenv("MEMREDUCT_CI") != NULL) {
        fprintf(stderr, "CI_MAIN_CONTENT_INSET unsupported=true gtk_major=%u root_type=%s holders=%u\n",
                gtk_get_major_version(), root_type != NULL ? root_type : "(null)", candidates);
        fflush(stderr);
    }

    g_list_free(children);
    return main_geometry_prepared;
}

/*
 * Mem Reduct v3.5.2's IDD_MAIN is a fixed-size dialog (no WS_THICKFRAME /
 * WS_SIZEBOX). Keep both production top-levels non-resizable so a tiling WM
 * cannot turn the provisional compact layout into a scaled-up canvas.
 */
int64_t mr_gui_lock_window_sizes(int64_t main_width, int64_t main_height,
                                 int64_t settings_width, int64_t settings_height)
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

    dump_window_tree(main_window, "main");
    dump_window_tree(settings_window_found, "settings");

    apply_decoration_layout();
    main_window_ref = main_window;
    settings_window_ref = settings_window_found;
    /* The UI library has already realized both unmapped toplevels by this
     * point. attach_header_bar() performs one safe unrealize/attach/realize
     * cycle so GTK accepts the custom titlebars without a warning. The idle
     * pass below is idempotent and still measures the first mapped geometry. */
    attach_header_bar(main_window, &main_decoration);
    attach_header_bar(settings_window_found, &settings_decoration);
    if (main_window != NULL || settings_window_found != NULL) {
        g_idle_add(on_first_idle, NULL);
    }

    if (main_window != NULL) {
        /* No gtk_window_set_default_size(): that call is what added 52px per
         * axis (see the contract above). */
        main_content_width = (int)main_width;
        main_content_height = (int)main_height;
        /* Pin before the window is mapped. Order matters: a resizable toplevel
         * that reaches the compositor without min==max hints gets tiled, and no
         * later resize brings it back (measured 941x983 -> stays 941x983).
         *
         * Geometry hints are measured on the WHOLE toplevel here, not on the
         * client area: pinning 337x369 produced a 337x322 client (the header bar
         * was taken out of it). So the pinned height is the content height plus
         * the header bar height, which is what makes the client land on 369. */
        int early_bar_h = 0;
        {
            GtkWidget *early_bar = gtk_window_get_titlebar(main_window);
            if (early_bar != NULL) {
                int min_h = 0;
                int nat_h = 0;
                gtk_widget_get_preferred_height(early_bar, &min_h, &nat_h);
                early_bar_h = nat_h > 0 ? nat_h : min_h;
            }
        }
        const int early_total_h = (int)main_height + early_bar_h;
        MrGdkGeometry early;
        memset(&early, 0, sizeof(early));
        early.min_width = (int)main_width;
        early.min_height = early_total_h;
        early.max_width = (int)main_width;
        early.max_height = early_total_h;
        early.base_width = (int)main_width;
        early.base_height = early_total_h;
        early.width_inc = 1;
        early.height_inc = 1;
        early.gravity = MR_GDK_GRAVITY_NORTH_WEST;
        gtk_window_set_geometry_hints(main_window, gtk_bin_get_child((GtkBin *)main_window), &early,
                                      MR_GDK_HINT_MIN_SIZE | MR_GDK_HINT_MAX_SIZE |
                                      MR_GDK_HINT_BASE_SIZE);
        /* Deliberately no gtk_window_resize() here: calling it right after the
         * hints re-negotiates and lands on 285x270 (measured). The hints alone
         * define the size.
         * Resizable so the maximize button does something; the hints above are
         * what keep the normal size at 337x369. */
        gtk_window_set_resizable(main_window, 1);
        prepare_main_geometry(main_window);
    }
    if (settings_window_found != NULL) {
        MrGdkGeometry settings_pin;
        memset(&settings_pin, 0, sizeof(settings_pin));
        settings_pin.min_width = (int)settings_width;
        settings_pin.min_height = (int)settings_height;
        settings_pin.max_width = (int)settings_width;
        settings_pin.max_height = (int)settings_height;
        settings_pin.base_width = (int)settings_width;
        settings_pin.base_height = (int)settings_height;
        settings_pin.width_inc = 1;
        settings_pin.height_inc = 1;
        settings_pin.gravity = MR_GDK_GRAVITY_NORTH_WEST;
        gtk_window_set_geometry_hints(settings_window_found, NULL, &settings_pin,
                                      MR_GDK_HINT_MIN_SIZE | MR_GDK_HINT_MAX_SIZE |
                                      MR_GDK_HINT_BASE_SIZE);
        gtk_window_resize(settings_window_found, (int)settings_width, (int)settings_height);
        /* The settings dialog is modal in v3.5.2 and has no maximize affordance,
         * so it keeps the fixed path. */
        gtk_window_set_resizable(settings_window_found, 0);
        settings_content_width = (int)settings_width;
        settings_content_height = (int)settings_height;
    }

    /* The main window is deliberately resizable so maximize works; its normal
     * size is held by geometry hints, not by the resizable flag. The settings
     * dialog keeps the fixed path because v3.5.2 has no maximize there. */
    const int main_ok = main_window != NULL && gtk_window_get_resizable(main_window);
    const int settings_fixed = settings_window_found != NULL && !gtk_window_get_resizable(settings_window_found);
    if (getenv("MEMREDUCT_CI") != NULL) {
        fprintf(stderr,
                "CI_WINDOW_SIZE_POLICY main_found=%d main_resizable=%d settings_found=%d settings_resizable=%d\n",
                main_window != NULL,
                main_window != NULL ? gtk_window_get_resizable(main_window) : -1,
                settings_window_found != NULL,
                settings_window_found != NULL ? gtk_window_get_resizable(settings_window_found) : -1);
        fflush(stderr);
    }
    g_list_free(windows);
    return main_ok && settings_fixed && main_geometry_prepared ? 1 : 0;
}

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

/*
 * Zamak 0.2.0's GTK std/ui creates one delete-event callback with each window.
 * Its callback destroys that window and returns TRUE, stopping later handlers.
 * Verify the GTK 3.24 runtime and the single pre-existing unblocked handler,
 * then block only that handler on the reusable settings window. The main
 * window and every other signal keep the UI library's lifecycle untouched.
 */
static int install_settings_delete_handler(GtkWindow *window, unsigned int signal_id)
{
    if (gtk_get_major_version() != 3 || gtk_get_minor_version() < 24) {
        return 0;
    }

    const unsigned int match_unblocked_id = (1u << 0) | (1u << 5);
    const unsigned long existing_close_handler = g_signal_handler_find(
        window, match_unblocked_id, signal_id, 0, NULL, NULL, NULL);
    if (existing_close_handler == 0) {
        return 0;
    }

    g_signal_handler_block(window, existing_close_handler);
    const unsigned long additional_handler = g_signal_handler_find(
        window, match_unblocked_id, signal_id, 0, NULL, NULL, NULL);
    if (additional_handler != 0) {
        g_signal_handler_unblock(window, existing_close_handler);
        return 0;
    }

    const unsigned long handler_id = g_signal_connect_data(
        (GtkWidget *)window,
        "delete-event",
        on_settings_delete,
        NULL,
        NULL,
        0);
    if (handler_id == 0) {
        g_signal_handler_unblock(window, existing_close_handler);
        return 0;
    }

    settings_preexisting_close_handler_id = existing_close_handler;
    settings_delete_signal_id = signal_id;
    settings_close_handler_id = handler_id;
    settings_window = window;
    settings_close_handler_connected = 1;
    return 1;
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
        if (delete_signal_id == 0 ||
            !install_settings_delete_handler(settings_window_found, delete_signal_id)) {
            if (getenv("MEMREDUCT_CI") != NULL) {
                fprintf(stderr,
                        "CI_NATIVE_SETTINGS_DELETE_HANDLER unsupported=true gtk=%u.%u.%u signal_id=%u\n",
                        gtk_get_major_version(), gtk_get_minor_version(), gtk_get_micro_version(),
                        delete_signal_id);
                fflush(stderr);
            }
            g_list_free(windows);
            return 0;
        }
        if (getenv("MEMREDUCT_CI") != NULL) {
            fprintf(stderr,
                    "CI_NATIVE_SETTINGS_DELETE_HANDLER signal_id=%u gtk=%u.%u.%u existing_handler=%lu single_preexisting_handler=1 blocked=1 connected=%d\n",
                    delete_signal_id,
                    gtk_get_major_version(), gtk_get_minor_version(), gtk_get_micro_version(),
                    settings_preexisting_close_handler_id,
                    settings_close_handler_id != 0);
            fflush(stderr);
        }
    }

    if (settings_window_found != NULL && settings_scroll_handler_id == 0 &&
        !install_settings_scroll_handler(settings_window_found)) {
        g_list_free(windows);
        return 0;
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
#ifdef MR_GUI_PRODUCTION
        (void)child_count;
        (void)child_index;
        (void)holder_type;
        (void)holder_name;
#endif
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
        !settings_close_handler_connected || settings_scroll_handler_id == 0) {
        g_list_free(windows);
        return 0;
    }

    settings_scroll_head = 0;
    settings_scroll_count = 0;
    settings_scroll_smooth_remainder = 0.0;
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

int64_t mr_gui_take_settings_scroll_steps(void)
{
    if (settings_scroll_count == 0) {
        return 0;
    }
    const SettingsScrollEvent event = settings_scroll_queue[settings_scroll_head];
    settings_scroll_head = (settings_scroll_head + 1) % SETTINGS_SCROLL_QUEUE_LIMIT;
    settings_scroll_count--;
    settings_scroll_taken_x = event.x;
    settings_scroll_taken_y = event.y;
    return event.delta;
}

int64_t mr_gui_settings_scroll_x(void)
{
    return settings_scroll_taken_x;
}

int64_t mr_gui_settings_scroll_y(void)
{
    return settings_scroll_taken_y;
}

int64_t mr_gui_settings_window_exists(void)
{
    return settings_window != NULL;
}

int64_t mr_gui_settings_close_handler_connected(void)
{
    return settings_close_handler_id != 0 && settings_preexisting_close_handler_id != 0 &&
           settings_delete_signal_id != 0;
}
