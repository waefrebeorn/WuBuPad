/* test_theme.c -- theming + status string + tab switching (Phase C).
 * Uses the headless backend (no display). Verifies theme persistence, status
 * string contents, and multi-doc tab activation. */
#include "ui/ui.h"
#include "ui/ui_headless.h"
#include "ui/ui_theme.h"
#include "doc.h"
#include "docs.h"

#include <stdio.h>
#include <string.h>

static int failures = 0;
#define CHECK(c, m) do { if(!(c)) { printf("FAIL: %s\n", m); failures++; } } while(0)

int main(void) {
    /* start from a known theme state (avoid cross-contamination from the
     * theme sub-test below which writes the persisted file). */
    UITheme *reset = ui_theme_create();
    ui_theme_set_dark(reset, 1);   /* dark */
    ui_theme_save(reset);
    ui_theme_free(reset);

    /* --- theme persistence --- */
    UITheme *t = ui_theme_create();
    CHECK(t != NULL, "theme create");
    ui_theme_set_dark(t, 0);
    CHECK(ui_theme_is_dark(t) == 0, "light set");
    ui_theme_save(t);
    UITheme *t2 = ui_theme_create();
    ui_theme_load(t2);
    CHECK(ui_theme_is_dark(t2) == 0, "light persisted+loaded");
    ui_theme_set_dark(t2, 1);
    ui_theme_save(t2);
    ui_theme_free(t); ui_theme_free(t2);

    /* --- multi-doc tab switching --- */
    Docs *docs = docs_create();
    size_t a = docs_open(docs, "a.c", "int a() {}\n", "c");
    size_t b = docs_open(docs, "b.c", "int b() {}\n", "c");
    CHECK(docs_count(docs) == 2, "two docs");
    docs_set_active(docs, a);
    CHECK(docs_active(docs) == a, "active == a");

    Doc *d = docs_doc(docs, a);
    UI *ui = ui_create(d, ui_headless_backend(), 80, 24);
    CHECK(ui != NULL, "ui create");
    ui_set_docs(ui, docs);
    CHECK(ui_theme_dark(ui) == 1, "default dark (reset file)");
    ui_next_tab(ui);
    CHECK(docs_active(docs) == b, "next tab -> b");
    ui_prev_tab(ui);
    CHECK(docs_active(docs) == a, "prev tab -> a");

    /* --- status string --- */
    char st[256];
    ui_status_string(ui, "c", st, sizeof st);
    CHECK(strstr(st, "Ln ") != NULL, "status has Ln");
    CHECK(strstr(st, "Col ") != NULL, "status has Col");
    CHECK(strstr(st, "dark") != NULL, "status has theme");

    /* --- theme toggle via UI flips + persists --- */
    int before = ui_theme_dark(ui);
    ui_toggle_theme(ui);
    CHECK(ui_theme_dark(ui) != before, "ui toggle flips theme");
    UITheme *probe = ui_theme_create();
    ui_theme_load(probe);
    CHECK(ui_theme_is_dark(probe) == ui_theme_dark(ui), "ui toggle persisted");
    ui_theme_free(probe);

    ui_free(ui);
    docs_free(docs);
    if (failures) { printf("FAILED (%d)\n", failures); return 1; }
    printf("PASS: theme/status/tabs (Phase C)\n");
    return 0;
}
