/* ui.c -- the UI controller: owns view state (scroll) and translates UI
 * commands into opaque Doc mutations, then renders through a backend.
 * No editing logic lives here; it all funnels through Doc (reuse-never-
 * duplicate). Clean C11, opaque. */
#include "ui.h"
#include "doc.h"
#include "lex.h"
#include "ui_headless.h"
#include "ui_find.h"
#include "ui_theme.h"
#include "ui_macro.h"
#include "complete.h"
#include "docs.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>

/* forward decl (defined later in this TU) */
static void ui_complete_reset(UI *ui);
/* ui_symbols is defined later in this TU; declared in ui.h but repeated here
 * so ui_render (defined earlier) sees it without relying on TU order. */
int ui_symbols(const UI *ui, DocSymbol **out, size_t *n);

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
    int colmode;        /* column/block selection active (mirrors doc) */
    UIMacro *macro;     /* optional input recorder (NULL = no macro) */
    UIFind *find;       /* find/replace controller (Phase B) */
    UITheme *theme;     /* active theme (Phase C) */
    Docs *docs;         /* multi-doc session (Phase C, NULL = single) */
    size_t fold[128];    /* folded ranges as pairs (a0,b0,a1,b1,...); even cnt */
    int    fold_n;       /* number of ranges (each uses 2 slots) */
    /* active completion session (Ctrl-Space cycling) */
    char  *comp_prefix;  /* word being completed */
    char **comp_list;    /* candidate list */
    size_t comp_n;       /* candidate count */
    int    comp_idx;     /* index of currently-shown candidate (-1 = none) */
    int    show_symbols; /* function-list panel toggle */
};

UI *ui_create(Doc *doc, const UI_Backend *be, int cols, int rows) {
    if (!doc || !be) return NULL;
    UI *ui = calloc(1, sizeof *ui);
    if (!ui) return NULL;
    ui->doc = doc;
    ui->be  = be;
    ui->cols = cols > 0 ? cols : 80;
    ui->rows = rows > 0 ? rows : 24;
    ui->comp_idx = -1;
    ui->find = ui_find_create();
    ui->theme = ui_theme_create();
    if (ui->theme) ui_theme_load(ui->theme);
    if (be->init(&ui->bstate, ui->cols, ui->rows) != 0) {
        ui_find_free(ui->find); ui_theme_free(ui->theme); free(ui); return NULL;
    }
    return ui;
}

void ui_free(UI *ui) {
    if (!ui) return;
    if (ui->find) ui_find_free(ui->find);
    if (ui->theme) ui_theme_free(ui->theme);
    ui_complete_reset(ui);
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

    int chrome = ui->be->chrome_rows ? ui->be->chrome_rows(ui->bstate) : 0;
    int tab_rows  = (chrome >= 2 && ui->be->draw_tab) ? 1 : 0;
    int status_row = (chrome >= 1 && ui->be->draw_status) ? ui->rows - 1 : -1;
    int text_top = tab_rows;
    int text_rows = ui->rows - tab_rows - (status_row >= 0 ? 1 : 0);

    /* tab strip */
    if (ui->be->draw_tab && ui->docs) {
        size_t n = docs_count(ui->docs), act = docs_active(ui->docs);
        for (size_t i = 0; i < n; i++) {
            const char *p = docs_path(ui->docs, i);
            const char *name = p ? p : "<untitled>";
            /* basename */
            const char *bn = name;
            for (const char *s = name; *s; s++) if (*s == '/') bn = s + 1;
            ui->be->draw_tab(ui->bstate, 0, (int)i, bn,
                            (int)i == (int)act, docs_dirty(ui->docs, i));
        }
    }

    for (int r = 0; r < text_rows; r++) {
        int view_row = text_top + r;
        /* find the next logical line to show at this visible row, honoring
         * folds (a folded range collapses to a single placeholder line). */
        size_t ln = (size_t)(ui->scroll_row + r);
        int placeholder = 0;
        if (ui->fold_n > 0) {
            /* advance ln to the next non-folded line (or a fold's first line) */
            while (ln < nlines && ui_is_folded(ui, ln)) {
                /* if ln is the start of a fold, show placeholder + skip range */
                int started = 0;
                for (int i = 0; i < ui->fold_n; i++) {
                    if (ui->fold[2*i] == ln) { started = 1; ln = ui->fold[2*i+1] + 1; break; }
                }
                if (!started) ln++;
                placeholder = started; /* set when we just emitted a placeholder */
                if (started) break;
            }
        }
        if (placeholder) {
            /* find the fold range we just collapsed to report its size */
            size_t cnt = 0;
            for (int i = 0; i < ui->fold_n; i++) {
                if (ui->fold[2*i+1] + 1 == ln) { cnt = ui->fold[2*i+1] - ui->fold[2*i] + 1; break; }
            }
            char ph[40];
            snprintf(ph, sizeof ph, "+ %zu lines folded", cnt);
            ui->be->draw_line(ui->bstate, view_row, ph, -1, TK_COMMENT);
            if (ui->be->draw_gutter) ui->be->draw_gutter(ui->bstate, view_row, (int)(ui->scroll_row + r) + 1);
            continue;
        }
        if (ln >= nlines) {
            ui->be->draw_line(ui->bstate, view_row, "", 0, TK_NONE);
            if (ui->be->draw_gutter) ui->be->draw_gutter(ui->bstate, view_row, 0);
            continue;
        }
        size_t start = doc_line_byte_start(ui->doc, ln);
        size_t next  = (ln + 1 < nlines) ? doc_line_byte_start(ui->doc, ln + 1) : total;
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
        ui->be->draw_line(ui->bstate, view_row, lp, llen, kind);
        if (ui->be->draw_gutter) ui->be->draw_gutter(ui->bstate, view_row, (int)(ln + 1));
    }

    /* caret (only if within text viewport) */
    size_t line, col, pos = doc_cursor(ui->doc);
    doc_line_col(ui->doc, pos, &line, &col);
    int crow = (int)line - ui->scroll_row + text_top;
    int ccol = (int)col - ui->scroll_col;
    if (crow >= text_top && crow < text_top + text_rows && ccol >= 0 && ccol < ui->cols)
        ui->be->draw_caret(ui->bstate, crow, ccol);

    /* status bar */
    if (status_row >= 0 && ui->be->draw_status) {
        char st[256];
        ui_status_string(ui, lang, st, sizeof st);
        ui->be->draw_status(ui->bstate, status_row, st);
    }

    /* function-list panel (right gutter) */
    if (ui_symbols_visible(ui) && ui->be->draw_symbols) {
        DocSymbol *syms = NULL; size_t n = 0;
        if (ui_symbols(ui, &syms, &n)) {
            for (size_t i = 0; i < n && (int)i < text_rows; i++)
                ui->be->draw_symbols(ui->bstate, text_top + (int)i,
                                    syms[i].name, (int)syms[i].line);
            doc_symbols_free(syms, n);
        }
    }

    ui->be->present(ui->bstate);
    free(text);
    if (lx) lex_free(lx);
}

/* Apply one input event (dispatch + scroll). Does NOT read from the backend
 * and does NOT render -- so macro replay can call it directly. Records the
 * event into the attached macro when recording. */
void ui_apply(UI *ui, char ch, int key) {
    if (!ui) return;
    /* any edit other than cycling completion cancels an active session */
    if (key != UI_KEY_COMPLETE) ui_complete_reset(ui);
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
        case UI_KEY_NEXTTAB: ui_next_tab(ui);    break;
        case UI_KEY_PREVTAB: ui_prev_tab(ui);    break;
        case UI_KEY_THEME:   ui_toggle_theme(ui); break;
        case UI_KEY_COLMODE: ui_toggle_colmode(ui); break;
        case UI_KEY_EOL:     ui_convert_eol(ui); break;
        case UI_KEY_MACRO:   ui_toggle_macro(ui); break;
        case UI_KEY_REPLAY:  ui_replay_macro(ui); break;
        case UI_KEY_COMPLETE: ui_complete(ui);   break;
        case UI_KEY_FOLD:     ui_fold_current_block(ui); break;
        case UI_KEY_SYMBOLS:  ui_toggle_symbols(ui); break;
        default:
            if (ch == '\n') ui_newline(ui);
            else if (ch) { char buf[2] = {ch,0}; ui_insert_text(ui, buf, 1); }
            break;
    }
    ui_scroll_to_cursor(ui);
    if (ui->macro && key != UI_KEY_MACRO && key != UI_KEY_REPLAY)
        ui_macro_add(ui->macro, ch, key);
}

/* Advance one input event: read a key, dispatch it, scroll, render.
 * Returns 0 if the session should continue, -1 if quit was requested. */
int ui_step(UI *ui, const char *lang) {
    if (!ui) return -1;
    char ch = 0; int key = UI_KEY_NONE;
    int rc = ui->be->get_key(ui->bstate, &ch, &key);
    if (rc != 0 || key == UI_KEY_QUIT) return -1;
    ui_apply(ui, ch, key);
    ui_render(ui, lang);
    return 0;
}

/* --- main loop --- */
int ui_run(UI *ui, const char *lang) {
    if (!ui) return -1;
    while (ui_step(ui, lang) == 0) { /* loop */ }
    return 0;
}

/* --- find / replace (Phase B): bind the DONE search engine to the Doc --- */
long ui_find(UI *ui, const char *pattern, int regex, int icase) {
    if (!ui) return -1;
    char *text = doc_text(ui->doc);
    size_t len = doc_length(ui->doc);
    long n = ui_find_run(ui->find, text, len, pattern, regex, icase);
    free(text);
    if (n > 0) {
        size_t s, e;
        if (ui_find_active(ui->find, &s, &e)) {
            doc_set_selection(ui->doc, s, e);
            doc_set_cursor(ui->doc, e);
            ui_scroll_to_cursor(ui);
        }
    }
    return n;
}

long ui_find_replace_all_in_doc(UI *ui, const char *repl) {
    if (!ui) return -1;
    char *text = doc_text(ui->doc);
    size_t len = doc_length(ui->doc);
    char *out; size_t nlen;
    long n = ui_find_replace_all(ui->find, text, len, repl, &out, &nlen);
    free(text);
    if (n >= 0 && out) {
        doc_replace(ui->doc, 0, len, out);
        free(out);
    } else free(out);
    return n;
}

void ui_find_next_match(UI *ui) {
    if (!ui) return;
    if (ui_find_next(ui->find)) {
        size_t s, e;
        if (ui_find_active(ui->find, &s, &e)) {
            doc_set_selection(ui->doc, s, e);
            doc_set_cursor(ui->doc, e);
            ui_scroll_to_cursor(ui);
        }
    }
}

long ui_find_matches(const UI *ui) {
    return ui && ui->find ? ui_find_count(ui->find) : 0;
}
int ui_find_error(const UI *ui) {
    return ui && ui->find ? ui_find_bad_pattern(ui->find) : 0;
}

/* --- theme (Phase C) --- */
void ui_set_theme(UI *ui, int dark) {
    if (!ui || !ui->theme) return;
    ui_theme_set_dark(ui->theme, dark);
    ui_theme_save(ui->theme);
    if (ui->be && ui->be->set_theme) ui->be->set_theme(ui->bstate, dark);
}
void ui_toggle_theme(UI *ui) {
    if (!ui || !ui->theme) return;
    ui_set_theme(ui, ui_theme_is_dark(ui->theme) ? 0 : 1);
}
int ui_theme_dark(const UI *ui) {
    return ui && ui->theme ? ui_theme_is_dark(ui->theme) : 1;
}

/* --- multi-doc session (Phase C) --- */
void ui_set_docs(UI *ui, Docs *docs) {
    if (!ui) return;
    ui->docs = docs;
    if (docs) {
        Doc *d = docs_doc(docs, docs_active(docs));
        if (d) ui->doc = d;
    }
}

void ui_status_string(const UI *ui, const char *lang, char *buf, size_t n) {
    if (!ui) { if (n) buf[0] = 0; return; }
    size_t line, col, pos = doc_cursor(ui->doc);
    doc_line_col(ui->doc, pos, &line, &col);
    long fm = ui_find_matches(ui);
    int dirty = ui->docs ? docs_dirty(ui->docs, docs_active(ui->docs)) : 0;
    snprintf(buf, n, " Ln %zu  Col %zu  %s  %s%s  [%ld match%s]",
             line + 1, col + 1,
             lang ? lang : "txt",
             ui_theme_dark(ui) ? "dark" : "light",
             dirty ? " *" : "",
             fm, fm == 1 ? "" : "es");
}

/* switch active document; updates ui->doc + resets scroll. */
static void ui_activate_doc(UI *ui, size_t i) {
    if (!ui->docs) return;
    if (i >= docs_count(ui->docs)) return;
    docs_set_active(ui->docs, i);
    Doc *d = docs_doc(ui->docs, i);
    if (d) { ui->doc = d; ui->scroll_row = 0; ui->scroll_col = 0; }
}
void ui_next_tab(UI *ui) {
    if (!ui->docs) return;
    size_t n = docs_count(ui->docs), a = docs_active(ui->docs);
    ui_activate_doc(ui, (a + 1) % n);
}
void ui_prev_tab(UI *ui) {
    if (!ui->docs) return;
    size_t n = docs_count(ui->docs), a = docs_active(ui->docs);
    ui_activate_doc(ui, (a + n - 1) % n);
}

/* --- macro (Phase D) --- */
void ui_set_macro(UI *ui, UIMacro *m) { if (ui) ui->macro = m; }
void ui_toggle_macro(UI *ui) {
    if (!ui || !ui->macro) return;
    if (ui_macro_recording(ui->macro)) ui_macro_record_stop(ui->macro);
    else ui_macro_record_start(ui->macro);
}
void ui_replay_macro(UI *ui) {
    if (!ui || !ui->macro) return;
    if (ui_macro_recording(ui->macro)) return;  /* don't nest */
    ui_macro_replay(ui, ui->macro);
    ui_render(ui, NULL);
}

/* --- column / EOL (Phase D) --- */
int ui_get_colmode(const UI *ui) {
    return ui && ui->doc ? doc_get_colmode(ui->doc) : 0;
}
void ui_toggle_colmode(UI *ui) {
    if (!ui || !ui->doc) return;
    int on = !doc_get_colmode(ui->doc);
    doc_set_colmode(ui->doc, on);
    ui->colmode = on;
}
void ui_convert_eol(UI *ui) {
    if (!ui || !ui->doc) return;
    doc_convert_eol(ui->doc, doc_eol_mode(ui->doc) ? 0 : 1);
}

/* --- folding (Phase D) --- */
int ui_is_folded(const UI *ui, size_t line) {
    if (!ui) return 0;
    for (int i = 0; i < ui->fold_n; i++)
        if (line >= ui->fold[2*i] && line <= ui->fold[2*i+1]) return 1;
    return 0;
}
static int fold_index(const UI *ui, size_t a, size_t b) {
    for (int i = 0; i < ui->fold_n; i++)
        if (ui->fold[2*i] == a && ui->fold[2*i+1] == b) return i;
    return -1;
}
int ui_toggle_fold(UI *ui, size_t a, size_t b) {
    if (!ui) return 0;
    if (a > b) { size_t t = a; a = b; b = t; }
    int idx = fold_index(ui, a, b);
    if (idx >= 0) {  /* remove */
        for (int i = idx; i + 1 < ui->fold_n; i++) {
            ui->fold[2*i] = ui->fold[2*(i+1)];
            ui->fold[2*i+1] = ui->fold[2*(i+1)+1];
        }
        ui->fold_n--;
        return 0;
    }
    if (ui->fold_n * 2 + 1 >= (int)(sizeof(ui->fold)/sizeof(ui->fold[0]))) return 0;
    ui->fold[2*ui->fold_n] = a;
    ui->fold[2*ui->fold_n+1] = b;
    ui->fold_n++;
    return 1;
}
void ui_unfold_all(UI *ui) { if (ui) ui->fold_n = 0; }

int ui_fold_current_block(UI *ui) {
    if (!ui || !ui->doc) return 0;
    char *text = doc_text(ui->doc);
    size_t len = strlen(text);
    size_t pos = doc_cursor(ui->doc);
    size_t cline, ccol; doc_line_col(ui->doc, pos, &cline, &ccol);
    /* find first '{' at or after the cursor line */
    size_t i = doc_line_byte_start(ui->doc, cline);
    int depth = 0, started = 0;
    size_t open_line = 0, close_line = 0;
    for (; i < len; i++) {
        if (text[i] == '{') {
            if (!started) { started = 1; depth = 1;
                size_t l; doc_line_col(ui->doc, i, &l, &(size_t){0}); open_line = l; }
            else depth++;
        } else if (text[i] == '}') {
            if (started) { depth--; if (depth == 0) {
                size_t l; doc_line_col(ui->doc, i, &l, &(size_t){0}); close_line = l; break; } }
        }
    }
    free(text);
    if (!started || close_line <= open_line) return 0;
    ui_toggle_fold(ui, open_line, close_line);
    return 1;
}

static void ui_complete_reset(UI *ui) {
    if (!ui) return;
    free(ui->comp_prefix);
    for (size_t i = 0; i < ui->comp_n; i++) free(ui->comp_list[i]);
    free(ui->comp_list);
    ui->comp_prefix = NULL;
    ui->comp_list = NULL;
    ui->comp_n = 0;
    ui->comp_idx = -1;
}

void ui_complete(UI *ui) {
    if (!ui || !ui->doc) return;

    /* Continue an active session: replace the current candidate with the next. */
    if (ui->comp_idx >= 0 && ui->comp_n > 0) {
        const char *cur = ui->comp_list[ui->comp_idx];
        size_t cur_len = strlen(cur);
        size_t pl = strlen(ui->comp_prefix);
        size_t ins_len = cur_len > pl ? cur_len - pl : 0;   /* suffix shown */
        size_t pos = doc_cursor(ui->doc);
        if (ins_len) doc_delete(ui->doc, pos - ins_len, ins_len);  /* remove shown */
        ui->comp_idx = (ui->comp_idx + 1) % (int)ui->comp_n;
        const char *next = ui->comp_list[ui->comp_idx];
        size_t nlen = strlen(next);
        size_t nins = nlen > pl ? nlen - pl : 0;
        if (nins) {
            char *suf = malloc(nins + 1);
            memcpy(suf, next + pl, nins);
            suf[nins] = 0;
            doc_insert(ui->doc, doc_cursor(ui->doc), suf, nins);
            free(suf);
        }
        ui_scroll_to_cursor(ui);
        return;
    }

    /* Start a new session. */
    char *text = doc_text(ui->doc);
    size_t len = strlen(text);
    size_t pos = doc_cursor(ui->doc);
    size_t start = pos;
    while (start > 0 && (isalnum((unsigned char)text[start-1]) || text[start-1] == '_'))
        start--;
    if (start == pos) { free(text); return; }   /* nothing to complete */
    size_t pl = pos - start;
    char *prefix = malloc(pl + 1);
    memcpy(prefix, text + start, pl);
    prefix[pl] = 0;
    size_t n = 0;
    char **c = doc_complete(text, len, prefix, &n);
    free(text);
    if (n == 0) { free(prefix); return; }
    /* stash for cycling */
    ui_complete_reset(ui);
    ui->comp_prefix = prefix;        /* transferred */
    ui->comp_list = c;
    ui->comp_n = n;
    ui->comp_idx = 0;
    const char *best = c[0];
    size_t best_len = strlen(best);
    size_t ins_len = best_len > pl ? best_len - pl : 0;
    if (ins_len) {
        char *suf = malloc(ins_len + 1);
        memcpy(suf, best + pl, ins_len);
        suf[ins_len] = 0;
        doc_insert(ui->doc, pos, suf, ins_len);
        free(suf);
    }
    ui_scroll_to_cursor(ui);
}

/* --- symbol / function list (Phase D) --- */
int ui_symbols(const UI *ui, DocSymbol **out, size_t *n) {
    if (!ui || !ui->doc) return 0;
    char *text = doc_text(ui->doc);
    size_t len = strlen(text);
    *out = doc_symbols(text, len, n);
    free(text);
    return 1;
}
void ui_function_list_string(const DocSymbol *syms, size_t n, char *buf, size_t bufn) {
    size_t off = 0;
    buf[0] = 0;
    for (size_t i = 0; i < n; i++) {
        int k = snprintf(buf + off, bufn - off, "%s : L%zu\n",
                         syms[i].name, syms[i].line + 1);
        if (k < 0) break;
        off += (size_t)k;
        if (off >= bufn) break;
    }
}
int ui_symbols_visible(const UI *ui) { return ui ? ui->show_symbols : 0; }
void ui_toggle_symbols(UI *ui) { if (ui) ui->show_symbols = !ui->show_symbols; }

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
