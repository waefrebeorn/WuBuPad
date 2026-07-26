/* test_editor.c -- editor features (Phase D): EOL convert, column/block
 * selection, macro record+replay. EOL + column tested on the Doc core
 * directly; macro tested through the headless UI (records real key events). */
#include "doc.h"
#include "ui/ui.h"
#include "ui/ui_headless.h"
#include "ui/ui_macro.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

static int failures = 0;
#define CHECK(c, m) do { if(!(c)) { printf("FAIL: %s\n", m); failures++; } } while(0)

static int streq(const char *a, const char *b) { return strcmp(a, b) == 0; }

int main(void) {
    /* --- EOL convert --- */
    Doc *d = doc_create("a\nb\nc");
    CHECK(doc_eol_mode(d) == 0, "default LF");
    doc_convert_eol(d, 1);
    CHECK(doc_eol_mode(d) == 1, "now CRLF mode");
    char *t = doc_text(d);
    CHECK(strstr(t, "\r\n") != NULL, "contains CRLF");
    /* every '\n' must be preceded by '\r' (no bare LF) */
    int bare = 0;
    for (int i = 1; t[i]; i++) if (t[i] == '\n' && t[i-1] != '\r') bare = 1;
    CHECK(bare == 0, "no bare LF");
    free(t);
    doc_undo(d); doc_undo(d);             /* undo EOL convert (2-step) */
    t = doc_text(d);
    CHECK(streq(t, "a\nb\nc"), "undo restores LF");
    free(t);
    doc_free(d);

    /* --- column / block selection --- */
    Doc *d2 = doc_create("abc\ndef\nghi");
    doc_set_colmode(d2, 1);
    doc_set_rect(d2, 0, 0, 2, 1);     /* rows 0..2, cols 0..1 */
    doc_delete_rect(d2);
    t = doc_text(d2);
    CHECK(streq(t, "bc\nef\nhi"), "column delete");
    free(t);
    doc_free(d2);

    Doc *d2b = doc_create("abc\ndef\nghi");
    doc_set_colmode(d2b, 1);
    doc_set_rect(d2b, 0, 0, 2, 1);    /* rows 0..2, cols 0..1 */
    doc_insert_rect(d2b, "X", 1);      /* prepend X to the column block */
    t = doc_text(d2b);
    CHECK(streq(t, "Xabc\nXdef\nXghi"), "column insert");
    free(t);
    doc_free(d2b);

    /* --- macro record + replay (headless UI) --- */
    Doc *d3 = doc_create("");
    UI *ui = ui_create(d3, ui_headless_backend(), 80, 24);
    UIMacro *m = ui_macro_create();
    ui_set_macro(ui, m);

    /* record: start, type 'x','y', stop */
    ui_headless_queue_key(ui, NULL, UI_KEY_MACRO);  /* start recording */
    ui_step(ui, NULL);
    CHECK(ui_macro_recording(m), "recording on");
    ui_headless_queue_key(ui, "x", UI_KEY_NONE);
    ui_step(ui, NULL);
    ui_headless_queue_key(ui, "y", UI_KEY_NONE);
    ui_step(ui, NULL);
    ui_headless_queue_key(ui, NULL, UI_KEY_MACRO);  /* stop recording */
    ui_step(ui, NULL);
    CHECK(!ui_macro_recording(m), "recording off");
    CHECK(ui_macro_len(m) == 2, "recorded 2 events");

    /* replay -> 'xy' appended again */
    ui_headless_queue_key(ui, NULL, UI_KEY_REPLAY);
    ui_step(ui, NULL);
    t = doc_text(d3);
    CHECK(streq(t, "xyxy"), "macro replay doubled input");
    free(t);

    ui_macro_free(m);
    ui_free(ui);
    doc_free(d3);

    if (failures) { printf("FAILED (%d)\n", failures); return 1; }
    printf("PASS: editor features (Phase D)\n");
    return 0;
}
