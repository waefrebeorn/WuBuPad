/* test_ui.c -- drive the editor core through the UI API on the headless
 * backend. Scripts keystrokes and asserts the rendered viewport. No platform
 * deps; runs green under ASan+UBSan.
 *
 * Editor semantics assumed (standard): typing inserts at the cursor and
 * advances it; backspace deletes the char BEFORE the cursor and leaves the
 * cursor there; delete removes the char AFTER the cursor. */
#include "ui/ui.h"
#include "ui/ui_headless.h"
#include "doc.h"
#include <assert.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

/* queue a key, then advance exactly one UI step (consume it + render) */
static void press(UI *ui, const char *s, int key) {
    ui_headless_queue_key(ui, s, key);
    ui_step(ui, NULL);
}

static void type(UI *ui, const char *s) {
    for (const char *p = s; *p; p++) press(ui, p, UI_KEY_NONE);
}

int main(void) {
    Doc *d = doc_create("");
    UI  *ui = ui_create(d, ui_headless_backend(), 20, 5);
    assert(ui && "ui_create");

    /* type "abc" -> caret at end */
    type(ui, "abc");
    assert(strcmp(ui_headless_line(ui, 0), "abc") == 0);

    /* move left twice, insert X -> aXbc, caret after X */
    press(ui, NULL, UI_KEY_LEFT);
    press(ui, NULL, UI_KEY_LEFT);
    press(ui, "X", UI_KEY_NONE);
    assert(strcmp(ui_headless_line(ui, 0), "aXbc") == 0);

    /* backspace the X -> abc, caret now before 'b' (pos 1) */
    press(ui, NULL, UI_KEY_BACKSPACE);
    assert(strcmp(ui_headless_line(ui, 0), "abc") == 0);
    /* insert at pos1 -> aQbc */
    press(ui, "Q", UI_KEY_NONE);
    assert(strcmp(ui_headless_line(ui, 0), "aQbc") == 0);
    /* get to end, then newline + "de" */
    press(ui, NULL, UI_KEY_END);
    press(ui, NULL, UI_KEY_ENTER);
    type(ui, "de");
    assert(strcmp(ui_headless_line(ui, 0), "aQbc") == 0);
    assert(strcmp(ui_headless_line(ui, 1), "de") == 0);

    /* undo: each typed char is one undo op, so two undos remove "de" */
    press(ui, NULL, UI_KEY_UNDO);
    press(ui, NULL, UI_KEY_UNDO);
    assert(strcmp(ui_headless_line(ui, 1), "") == 0);
    /* redo: two redos bring "de" back */
    press(ui, NULL, UI_KEY_REDO);
    press(ui, NULL, UI_KEY_REDO);
    assert(strcmp(ui_headless_line(ui, 1), "de") == 0);

    /* caret down then home/end on line 1 */
    press(ui, NULL, UI_KEY_DOWN);
    press(ui, NULL, UI_KEY_HOME);
    press(ui, "Z", UI_KEY_NONE);
    assert(strcmp(ui_headless_line(ui, 1), "Zde") == 0);
    press(ui, NULL, UI_KEY_END);
    press(ui, "Q", UI_KEY_NONE);
    assert(strcmp(ui_headless_line(ui, 1), "ZdeQ") == 0);

    /* delete char after caret: from END, go left once, DEL removes 'Q' */
    press(ui, NULL, UI_KEY_LEFT);
    press(ui, NULL, UI_KEY_DEL);
    assert(strcmp(ui_headless_line(ui, 1), "Zde") == 0);

    char *txt = doc_text(d);
    assert(strcmp(txt, "aQbc\nZde") == 0);
    free(txt);

    ui_free(ui);
    doc_free(d);
    printf("test_ui: PASS\n");
    return 0;
}
