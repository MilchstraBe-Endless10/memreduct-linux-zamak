/* SPDX-License-Identifier: GPL-3.0-or-later */
#define _POSIX_C_SOURCE 200809L
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

/* Keep the test-only font probe header-free like the production GTK bridge. */
typedef struct _GtkWidget GtkWidget;
typedef struct _GtkWindow GtkWindow;
typedef struct _GdkWindow GdkWindow;
typedef struct _GdkRectangle GdkRectangle;
typedef struct _GtkStyleContext GtkStyleContext;
typedef struct _GList GList;
typedef struct _PangoFontDescription PangoFontDescription;
typedef struct _PangoContext PangoContext;
typedef struct _PangoFontMap PangoFontMap;
typedef struct _PangoFont PangoFont;
typedef struct _PangoFontMetrics PangoFontMetrics;
typedef struct _PangoLanguage PangoLanguage;
typedef struct _cairo cairo_t;
typedef struct _cairo_surface cairo_surface_t;

struct _GList {
    void *data;
    GList *next;
    GList *prev;
};

struct _GdkRectangle {
    int x;
    int y;
    int width;
    int height;
};

extern GtkWidget *gtk_label_new(const char *text);
extern GtkStyleContext *gtk_widget_get_style_context(GtkWidget *widget);
extern void gtk_style_context_get(GtkStyleContext *context, int state, const char *first_property, ...);
extern PangoContext *gtk_widget_get_pango_context(GtkWidget *widget);
extern PangoFontDescription *pango_font_description_copy(const PangoFontDescription *description);
extern void pango_font_description_set_size(PangoFontDescription *description, int size);
extern PangoFontMap *pango_context_get_font_map(PangoContext *context);
extern PangoFont *pango_font_map_load_font(PangoFontMap *font_map, PangoContext *context, const PangoFontDescription *description);
extern PangoFontMetrics *pango_context_get_metrics(PangoContext *context, const PangoFontDescription *description, PangoLanguage *language);
extern PangoLanguage *pango_language_from_string(const char *language);
extern PangoFontDescription *pango_font_describe(PangoFont *font);
extern char *pango_font_description_to_string(const PangoFontDescription *description);
extern const char *pango_font_description_get_family(const PangoFontDescription *description);
extern int pango_font_description_get_weight(const PangoFontDescription *description);
extern int pango_font_metrics_get_ascent(PangoFontMetrics *metrics);
extern int pango_font_metrics_get_descent(PangoFontMetrics *metrics);
extern void pango_font_metrics_unref(PangoFontMetrics *metrics);
extern void pango_font_description_free(PangoFontDescription *description);
extern void gtk_widget_destroy(GtkWidget *widget);
extern GList *gtk_window_list_toplevels(void);
extern const char *gtk_window_get_title(GtkWindow *window);
extern int gtk_widget_get_allocated_width(GtkWidget *widget);
extern int gtk_widget_get_allocated_height(GtkWidget *widget);
extern int gtk_widget_get_visible(GtkWidget *widget);
extern int gtk_widget_get_mapped(GtkWidget *widget);
extern int gtk_widget_get_scale_factor(GtkWidget *widget);
extern GdkWindow *gtk_widget_get_window(GtkWidget *widget);
extern void gtk_window_get_size(GtkWindow *window, int *width, int *height);
extern int gtk_window_get_decorated(GtkWindow *window);
extern void gdk_window_get_frame_extents(GdkWindow *window, GdkRectangle *rect);
extern void gtk_widget_draw(GtkWidget *widget, cairo_t *cr);
extern cairo_surface_t *cairo_image_surface_create(int format, int width, int height);
extern cairo_t *cairo_create(cairo_surface_t *target);
extern void cairo_destroy(cairo_t *cr);
extern int cairo_surface_write_to_png_stream(
    cairo_surface_t *surface,
    int (*write_func)(void *closure, const unsigned char *data, unsigned int length),
    void *closure);
extern void cairo_surface_destroy(cairo_surface_t *surface);
extern unsigned int g_timeout_add(unsigned int interval, int (*function)(void *), void *data);
extern void g_list_free(GList *list);
extern void *g_object_ref_sink(void *object);
extern void g_object_unref(void *object);
extern void g_free(void *memory);

#define PANGO_SCALE 1024

/* Test controls are inert unless the CI runner sets these exact variables. */
long long mr_gui_test_mode(void)
{
    const char *value = getenv("MEMREDUCT_CI");
    return value != NULL && strcmp(value, "1") == 0 ? 1 : 0;
}

long long mr_gui_test_selected_group(void)
{
    const char *value = getenv("MEMREDUCT_CI_SELECTED_GROUP");
    char *end = NULL;
    long parsed;

    if (mr_gui_test_mode() == 0 || value == NULL || value[0] == '\0') {
        return -1;
    }
    errno = 0;
    parsed = strtol(value, &end, 10);
    if (errno != 0 || end == value || *end != '\0' || parsed < 0 || parsed > 2) {
        return -1;
    }
    return parsed;
}

long long mr_gui_test_timeout_ms(void)
{
    const char *value = getenv("MEMREDUCT_CI_EXIT_MS");
    char *end = NULL;
    long parsed;

    if (value == NULL || value[0] == '\0') {
        return 0;
    }

    errno = 0;
    parsed = strtol(value, &end, 10);
    if (errno != 0 || end == value || *end != '\0' || parsed < 1000 || parsed > 600000) {
        return 0;
    }
    return (long long)parsed;
}

static int capture_write(void *closure, const unsigned char *data, unsigned int length)
{
    int fd = *(int *)closure;

    while (length > 0) {
        ssize_t written = write(fd, data, length);
        if (written < 0) {
            if (errno == EINTR) {
                continue;
            }
            return 11; /* CAIRO_STATUS_WRITE_ERROR */
        }
        if (written == 0) {
            return 11;
        }
        data += written;
        length -= (unsigned int)written;
    }
    return 0;
}

static int capture_window(GtkWindow *window, const char *kind)
{
    GtkWidget *widget = (GtkWidget *)window;
    const int visible = gtk_widget_get_visible(widget);
    const int mapped = gtk_widget_get_mapped(widget);
    const int scale_factor = gtk_widget_get_scale_factor(widget);
    const int width = gtk_widget_get_allocated_width(widget);
    const int height = gtk_widget_get_allocated_height(widget);
    int client_width = 0;
    int client_height = 0;
    GdkRectangle frame = {0, 0, 0, 0};
    char path[] = "/tmp/memreduct-zamak-window-XXXXXX";
    int fd;
    cairo_surface_t *surface;
    cairo_t *cr;
    int status;

    if (!visible || !mapped || width <= 0 || height <= 0) {
        fprintf(stderr,
                "CI_CAPTURE kind=%s status=skipped visible=%d mapped=%d width=%d height=%d reason=window-not-ready\n",
                kind, visible, mapped, width, height);
        return 0;
    }

    gtk_window_get_size(window, &client_width, &client_height);
    GdkWindow *gdk_window = gtk_widget_get_window(widget);
    if (gdk_window != NULL) {
        gdk_window_get_frame_extents(gdk_window, &frame);
    }
    fprintf(stderr,
            "CI_WINDOW_GEOMETRY kind=%s allocated=%dx%d client=%dx%d frame=%dx%d scale_factor=%d decorated=%d\n",
            kind, width, height, client_width, client_height,
            frame.width, frame.height, scale_factor, gtk_window_get_decorated(window));
    fflush(stderr);

    fd = mkstemp(path);
    if (fd < 0) {
        fprintf(stderr, "CI_CAPTURE kind=%s status=mkstemp-failed errno=%d\n", kind, errno);
        return 0;
    }
    surface = cairo_image_surface_create(0, width, height); /* CAIRO_FORMAT_ARGB32 */
    cr = cairo_create(surface);
    gtk_widget_draw(widget, cr);
    cairo_destroy(cr);
    status = cairo_surface_write_to_png_stream(surface, capture_write, &fd);
    cairo_surface_destroy(surface);
    if (close(fd) != 0 && status == 0) {
        status = 11;
    }
    if (status != 0) {
        (void)unlink(path);
        fprintf(stderr, "CI_CAPTURE kind=%s status=png-write-failed code=%d\n", kind, status);
        return 0;
    }

    fprintf(stderr, "CI_CAPTURE kind=%s status=ok path=%s width=%d height=%d\n", kind, path, width, height);
    fflush(stderr);
    return 1;
}

static int capture_application_windows(void *data)
{
    GList *windows;
    int captured = 0;
    (void)data;

    if (mr_gui_test_mode() == 0 || getenv("MEMREDUCT_CI_CAPTURE") == NULL ||
        strcmp(getenv("MEMREDUCT_CI_CAPTURE"), "1") != 0) {
        return 0;
    }

    windows = gtk_window_list_toplevels();
    for (GList *item = windows; item != NULL; item = item->next) {
        GtkWindow *window = (GtkWindow *)item->data;
        const char *title = gtk_window_get_title(window);
        if (title == NULL) {
            continue;
        }
        if (strcmp(title, "Mem Reduct") == 0) {
            captured += capture_window(window, "main");
        } else if (strcmp(title, "Settings - Mem Reduct") == 0) {
            captured += capture_window(window, "settings");
        }
    }
    g_list_free(windows);
    if (captured == 0) {
        fputs("CI_CAPTURE status=no-application-window-captured\n", stderr);
        fflush(stderr);
    }
    return 0;
}

long long mr_gui_test_schedule_capture(void)
{
    if (mr_gui_test_mode() == 0 || getenv("MEMREDUCT_CI_CAPTURE") == NULL ||
        strcmp(getenv("MEMREDUCT_CI_CAPTURE"), "1") != 0) {
        return 0;
    }
    return g_timeout_add(1000, capture_application_windows, NULL) != 0 ? 1 : 0;
}

long long mr_gui_test_fail_sample_after(long long elapsed_ms)
{
    const char *value = getenv("MEMREDUCT_CI_FAIL_SAMPLE_AFTER_MS");
    char *end = NULL;
    long long parsed;

    if (mr_gui_test_mode() == 0 || value == NULL || value[0] == '\0') {
        return 0;
    }
    errno = 0;
    parsed = strtoll(value, &end, 10);
    if (errno != 0 || end == value || *end != '\0' || parsed < 3000 || parsed > 120000) {
        return 0;
    }
    return elapsed_ms >= parsed ? 1 : 0;
}

long long mr_gui_test_start_settings(void)
{
    const char *value = getenv("MEMREDUCT_CI_START_SETTINGS");
    return mr_gui_test_mode() != 0 && value != NULL && strcmp(value, "1") == 0 ? 1 : 0;
}

long long mr_gui_test_log_canvas_font(void)
{
    if (mr_gui_test_mode() == 0) {
        return 0;
    }

    GtkWidget *probe = gtk_label_new("");
    g_object_ref_sink(probe);
    GtkStyleContext *style = gtk_widget_get_style_context(probe);
    PangoFontDescription *base = NULL;
    gtk_style_context_get(style, 0, "font", &base, NULL); /* GTK_STATE_FLAG_NORMAL */
    if (base == NULL) {
        gtk_widget_destroy(probe);
        g_object_unref(probe);
        fputs("CI_GTK_FONT status=unavailable reason=no-label-font\n", stderr);
        return 0;
    }

    PangoFontDescription *canvas_font = pango_font_description_copy(base);
    pango_font_description_set_size(canvas_font, 8 * PANGO_SCALE);
    PangoContext *context = gtk_widget_get_pango_context(probe);
    PangoFont *resolved = pango_font_map_load_font(
        pango_context_get_font_map(context), context, canvas_font);
    PangoFontMetrics *metrics = pango_context_get_metrics(
        context, canvas_font, pango_language_from_string("en"));
    PangoFontDescription *resolved_description = resolved != NULL
        ? pango_font_describe(resolved)
        : NULL;
    char *base_text = pango_font_description_to_string(base);
    const char *family = pango_font_description_get_family(base);
    const char *resolved_family = resolved_description != NULL
        ? pango_font_description_get_family(resolved_description)
        : "unresolved";

    fprintf(stderr,
            "CI_GTK_FONT source=GtkLabelStyle family=\"%s\" resolved=\"%s\" base=\"%s\" canvas_size_pt=8 weight=%d ascent_px=%.2f descent_px=%.2f\n",
            family != NULL ? family : "unknown",
            resolved_family != NULL ? resolved_family : "unknown",
            base_text != NULL ? base_text : "unknown",
            pango_font_description_get_weight(canvas_font),
            metrics != NULL ? (double)pango_font_metrics_get_ascent(metrics) / PANGO_SCALE : 0.0,
            metrics != NULL ? (double)pango_font_metrics_get_descent(metrics) / PANGO_SCALE : 0.0);
    fflush(stderr);

    if (metrics != NULL) pango_font_metrics_unref(metrics);
    if (resolved_description != NULL) pango_font_description_free(resolved_description);
    if (resolved != NULL) g_object_unref(resolved);
    pango_font_description_free(canvas_font);
    pango_font_description_free(base);
    g_free(base_text);
    gtk_widget_destroy(probe);
    g_object_unref(probe);
    return 1;
}
