/* ui.c -- the UI controller: owns view state (scroll) and translates UI
 * commands into opaque Doc mutations, then renders through a backend.
 * No editing logic lives here; it all funnels through Doc (reuse-never-
 * duplicate). Clean C11, opaque. */
#include "ui.h"
#include "doc.h"
#include "lex.h"
#include "ui_headless.h"
#include <stdlib.h>
#include <string.h>

/* declared in ui_headless.c */
const UI_Backend *ui_headless_backend(void);
/* declared in ui_tty.c */
const UI_Backend *ui_tty_backend(void);
int ui__tty_enable_raw(void *bstate);
/* declared in ui_gfx.c */
const UI_Backend *ui_gfx_backend(void);

struct UI {
    Doc *doc;
    const UI_Backend *be;
    void *bstate;
    int cols, rows;
    int scroll_row;     /* top visible line */
    int scroll_col;     /* left visible column (horizontal) */
};

UI *ui_create(Doc *doc, const UI_Backend *be, int cols, int rows) {
    if (!doc || !be) return NULL;
    UI *ui = calloc(1, sizeof *ui);
    if (!ui) return NULL;
    ui->doc = doc;
    ui->be  = be;
    ui->cols = cols > 0 ? cols : 80;
    ui->rows = rows > 0 ? rows : 24;
    if (be->init(&ui->bstate, ui->cols, ui->rows) != 0) { free(ui); return NULL; }
    return ui;
}

void ui_free(UI *ui) {
    if (!ui) return;
    if (ui->be && ui->be->destroy) ui->be->destroy(ui->bstate);
    free(ui);
}

void ui_resize(UI *ui, int cols, int rows) {
    if (!ui) return;
    ui->cols = cols; ui->rows = rows;
    if (ui->be && ui->be->resize) ui->be->resize(ui->bstate, &ui->cols, &ui->rows);
}

void ui_get_size(const UI *ui, int *cols, int *rows) {
    if (!ui) return;
    if (cols) *cols = ui->cols;
    if (rows) *rows = ui->rows;
}

/* --- editing commands (delegate to Doc) --- */
void ui_insert_text(UI *ui, const char *text, size_t len) {
    if (!ui || !text) return;
    doc_type(ui->doc, text, len);
}
void ui_backspace(UI *ui) {
    if (!ui) return;
    size_t c = doc_cursor(ui->doc);
    if (c == 0) return;
    doc_delete(ui->doc, c - 1, 1);
}
void ui_delete(UI *ui) {
    if (!ui) return;
    size_t c = doc_cursor(ui->doc);
    if (c >= doc_length(ui->doc)) return;
    doc_delete(ui->doc, c, 1);
}
void ui_newline(UI *ui) {
    if (!ui) return;
    doc_type(ui->doc, "\n", 1);
}
void ui_cursor_left(UI *ui) {
    if (!ui || doc_cursor(ui->doc) == 0) return;
    doc_set_cursor(ui->doc, doc_cursor(ui->doc) - 1);
}
void ui_cursor_right(UI *ui) {
    if (!ui) return;
    size_t c = doc_cursor(ui->doc);
    if (c >= doc_length(ui->doc)) return;
    doc_set_cursor(ui->doc, c + 1);
}
void ui_cursor_up(UI *ui) {
    if (!ui) return;
    size_t line, col, pos = doc_cursor(ui->doc);
    doc_line_col(ui->doc, pos, &line, &col);
    if (line == 0) return;
    size_t above = doc_line_byte_start(ui->doc, line - 1);
    size_t above_len = doc_line_byte_start(ui->doc, line) - above;
    size_t newcol = col < above_len ? col : above_len;
    doc_set_cursor(ui->doc, above + newcol);
}
void ui_cursor_down(UI *ui) {
    if (!ui) return;
    size_t line, col, pos = doc_cursor(ui->doc);
    doc_line_col(ui->doc, pos, &line, &col);
    size_t nlines = doc_lines(ui->doc);
    if (line + 1 >= nlines) return;
    size_t cur = doc_line_byte_start(ui->doc, line);
    size_t cur_len = doc_line_byte_start(ui->doc, line + 1) - cur;
    size_t below = doc_line_byte_start(ui->doc, line + 1);
    size_t below_len = (line + 2 < nlines)
        ? doc_line_byte_start(ui->doc, line + 2) - below
        : doc_length(ui->doc) - below;
    size_t newcol = col < below_len ? col : below_len;
    (void)cur_len;
    doc_set_cursor(ui->doc, below + newcol);
}
void ui_cursor_home(UI *ui) {
    if (!ui) return;
    size_t line, col, pos = doc_cursor(ui->doc);
    doc_line_col(ui->doc, pos, &line, &col);
    doc_set_cursor(ui->doc, doc_line_byte_start(ui->doc, line));
}
void ui_cursor_end(UI *ui) {
    if (!ui) return;
    size_t line, col, pos = doc_cursor(ui->doc);
    doc_line_col(ui->doc, pos, &line, &col);
    size_t cur = doc_line_byte_start(ui->doc, line);
    size_t cur_len = (line + 1 < doc_lines(ui->doc))
        ? doc_line_byte_start(ui->doc, line + 1) - cur
        : doc_length(ui->doc) - cur;
    doc_set_cursor(ui->doc, cur + cur_len);
}
void ui_page_up(UI *ui) {
    if (!ui) return;
    for (int i = 0; i < ui->rows - 1; i++) ui_cursor_up(ui);
}
void ui_page_down(UI *ui) {
    if (!ui) return;
    for (int i = 0; i < ui->rows - 1; i++) ui_cursor_down(ui);
}
void ui_undo(UI *ui) { if (ui && doc_can_undo(ui->doc)) doc_undo(ui->doc); }
void ui_redo(UI *ui) { if (ui && doc_can_redo(ui->doc)) doc_redo(ui->doc); }

void ui_scroll_to_cursor(UI *ui) {
    if (!ui) return;
    size_t line, col, pos = doc_cursor(ui->doc);
    doc_line_col(ui->doc, pos, &line, &col);
    if ((int)line < ui->scroll_row) ui->scroll_row = (int)line;
    if ((int)line >= ui->scroll_row + ui->rows)
        ui->scroll_row = (int)line - (ui->rows - 1);
    if ((int)col < ui->scroll_col) ui->scroll_col = (int)col;
    if ((int)col >= ui->scroll_col + ui->cols)
        ui->scroll_col = (int)col - (ui->cols - 1);
}

/* --- render --- */
void ui_render(UI *ui, const char *lang) {
    if (!ui) return;
    char *text = doc_text(ui->doc);
    Lex *lx = lang ? lex_create(lang) : NULL;
    size_t nlines = doc_lines(ui->doc);
    size_t total = doc_length(ui->doc);
    (void)total;

    for (int r = 0; r < ui->rows; r++) {
        size_t ln = (size_t)(ui->scroll_row + r);
        if (ln >= nlines) { ui->be->draw_line(ui->bstate, r, "", 0, TK_NONE); continue; }
        size_t start = doc_line_byte_start(ui->doc, ln);
        size_t next  = (ln + 1 < nlines) ? doc_line_byte_start(ui->doc, ln + 1) : total;
        /* strip trailing newline from the line */
        size_t len = next - start;
        while (len > 0 && (text[start + len - 1] == '\n' || text[start + len - 1] == '\r'))
            len--;
        const char *lp = text + start + (size_t)ui->scroll_col;
        int   llen = (int)len - ui->scroll_col;
        if (llen < 0) llen = 0;
        int kind = TK_NONE;
        if (lx) {
            LexSpan spans[8];
            size_t ns = lex_run(lx, text + start, len, spans, 8);
            size_t cur = (size_t)ui->scroll_col;
            for (size_t s = 0; s < ns; s++) {
                if (spans[s].start <= cur && cur < spans[s].end) { kind = (int)spans[s].kind; break; }
            }
        }
        ui->be->draw_line(ui->bstate, r, lp, llen, kind);
    }
    /* caret */
    size_t line, col, pos = doc_cursor(ui->doc);
    doc_line_col(ui->doc, pos, &line, &col);
    int crow = (int)line - ui->scroll_row;
    int ccol = (int)col - ui->scroll_col;
    if (crow >= 0 && crow < ui->rows && ccol >= 0 && ccol < ui->cols)
        ui->be->draw_caret(ui->bstate, crow, ccol);
    ui->be->present(ui->bstate);
    free(text);
    if (lx) lex_free(lx);
}

/* Advance one input event: read a key, dispatch it, scroll, render.
 * Returns 0 if the session should continue, -1 if quit was requested. */
int ui_step(UI *ui, const char *lang) {
    if (!ui) return -1;
    char ch = 0; int key = UI_KEY_NONE;
    int rc = ui->be->get_key(ui->bstate, &ch, &key);
    if (rc != 0 || key == UI_KEY_QUIT) return -1;
    switch (key) {
        case UI_KEY_LEFT:   ui_cursor_left(ui);  break;
        case UI_KEY_RIGHT:  ui_cursor_right(ui); break;
        case UI_KEY_UP:     ui_cursor_up(ui);    break;
        case UI_KEY_DOWN:   ui_cursor_down(ui);  break;
        case UI_KEY_HOME:   ui_cursor_home(ui);  break;
        case UI_KEY_END:    ui_cursor_end(ui);   break;
        case UI_KEY_PGUP:   ui_page_up(ui);      break;
        case UI_KEY_PGDOWN: ui_page_down(ui);    break;
        case UI_KEY_BACKSPACE: ui_backspace(ui); break;
        case UI_KEY_DEL:    ui_delete(ui);       break;
        case UI_KEY_ENTER:  ui_newline(ui);      break;
        case UI_KEY_UNDO:   ui_undo(ui);         break;
        case UI_KEY_REDO:   ui_redo(ui);         break;
        default:
            if (ch == '\n') ui_newline(ui);
            else if (ch) { char buf[2] = {ch,0}; ui_insert_text(ui, buf, 1); }
            break;
    }
    ui_scroll_to_cursor(ui);
    ui_render(ui, lang);
    return 0;
}

/* --- main loop --- */
int ui_run(UI *ui, const char *lang) {
    if (!ui) return -1;
    while (ui_step(ui, lang) == 0) { /* loop */ }
    return 0;
}

/* --- headless backend accessors (for tests) ------------------------- */
/* These live here because UI is a complete type only in this TU. They reach
 * into the headless backend's state, which ui_headless.c owns. We keep the
 * queue/line logic in ui_headless.c as static helpers and expose them via a
 * small internal header-free contract: ui_headless_backend() identity check
 * + bstate cast. To avoid duplicating logic, ui_headless.c exposes the static
 * HL push/peek through these two functions implemented here. */
int ui_headless_queue_key(UI *ui, const char *ch, int key) {
    if (!ui || ui->be != ui_headless_backend()) return -1;
    /* HL layout is private to ui_headless.c; ask it directly. */
    return ui__hl_queue(ui->bstate, ch, key);
}

const char *ui_headless_line(UI *ui, int row) {
    if (!ui || ui->be != ui_headless_backend()) return "";
    return ui__hl_line(ui->bstate, row);
}

/* --- TTY backend: enter raw mode (platform code lives in ui_tty.c) ----- */
int ui_tty_enable_raw(UI *ui) {
    if (!ui || ui->be != ui_tty_backend()) return -1;
    /* ui_tty.c owns TTY; ask it to flip the terminal to raw mode. */
    return ui__tty_enable_raw(ui->bstate);
}
