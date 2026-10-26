/* SPDX-License-Identifier: GPL-3.0-or-later */
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Keep the test-only font probe header-free like the production GTK bridge. */
typedef struct _GtkWidget GtkWidget;
typedef struct _GtkStyleContext GtkStyleContext;
typedef struct _PangoFontDescription PangoFontDescription;
typedef struct _PangoContext PangoContext;
typedef struct _PangoFontMap PangoFontMap;
typedef struct _PangoFont PangoFont;
typedef struct _PangoFontMetrics PangoFontMetrics;
typedef struct _PangoLanguage PangoLanguage;

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
    if (errno != 0 || end == value || *end != '\0' || parsed < 1000 || parsed > 120000) {
        return 0;
    }
    return (long long)parsed;
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
