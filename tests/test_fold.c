/* test_fold.c -- code folding (Phase D, part 3). Verifies fold toggle,
 * is_folded query, unfold-all, block folding via brace matching, and that the
 * headless renderer collapses a folded range into one placeholder line. */
#include "ui/ui.h"
#include "ui/ui_headless.h"
#include "doc.h"

#include <stdio.h>
#include <string.h>

static int failures = 0;
#define CHECK(c, m) do { if(!(c)) { printf("FAIL: %s\n", m); failures++; } } while(0)

int main(void) {
    const char *src =
        "int main() {\n"      /* 0 */
        "    int a = 1;\n"     /* 1 */
        "    int b = 2;\n"     /* 2 */
        "    return a + b;\n" /* 3 */
        "}\n";                /* 4 */
    Doc *d = doc_create(src);
    UI *ui = ui_create(d, ui_headless_backend(), 80, 24);

    /* manual fold of lines 1..3 */
    int r = ui_toggle_fold(ui, 1, 3);
    CHECK(r == 1, "fold created");
    CHECK(ui_is_folded(ui, 1), "line 1 folded");
    CHECK(ui_is_folded(ui, 2), "line 2 folded");
    CHECK(ui_is_folded(ui, 3), "line 3 folded");
    CHECK(!ui_is_folded(ui, 0), "line 0 not folded");

    /* render collapses the range into one placeholder */
    ui_render(ui, "c");
    /* line 0 visible, line 1..3 collapsed -> placeholder at row 1, line 2 visible */
    const char *row0 = ui_headless_line(ui, 0);
    const char *row1 = ui_headless_line(ui, 1);
    const char *row2 = ui_headless_line(ui, 2);
    CHECK(strstr(row0, "int main") != NULL, "row0 shows main");
    CHECK(strstr(row1, "folded") != NULL, "row1 is the fold placeholder");
    CHECK(strstr(row2, "return a + b") == NULL, "row2 is NOT the hidden line");
    CHECK(strstr(row2, "}") != NULL, "row2 shows closing brace after fold");

    /* toggle off */
    r = ui_toggle_fold(ui, 1, 3);
    CHECK(r == 0, "fold removed");
    CHECK(!ui_is_folded(ui, 2), "line 2 no longer folded");

    /* block fold via brace matching: cursor on line 0 (int main() {) */
    doc_set_cursor(d, 0);
    int fb = ui_fold_current_block(ui);
    CHECK(fb == 1, "block fold succeeded");
    CHECK(ui_is_folded(ui, 3), "block fold hides inner line");

    ui_unfold_all(ui);
    CHECK(!ui_is_folded(ui, 1), "unfold_all clears folds");

    ui_free(ui);
    doc_free(d);
    if (failures) { printf("FAILED (%d)\n", failures); return 1; }
    printf("PASS: folding (Phase D)\n");
    return 0;
}
