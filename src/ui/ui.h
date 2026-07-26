/* ui.h -- thin platform abstraction over the WuBuPad document core.
 *
 * The UI layer NEVER re-implements editing. It owns only view state
 * (scroll offset, dirty-region bookkeeping) and delegates all mutations
 * to the opaque Doc. A backend implements the small vtable below; the
 * same core backs a headless recorder, a terminal editor, or a real
 * window. Core never includes platform headers. Clean C11, opaque. */
#ifndef WUBUPAD_UI_H
#define WUBUPAD_UI_H

#include <stddef.h>

typedef struct Doc Doc;   /* opaque; editor core */
typedef struct Docs Docs; /* opaque; multi-doc session */
typedef struct UI  UI;
typedef struct UIMacro UIMacro; /* opaque; input macro */

/* A backend draws spans and reports input. Implemented per-platform. */
typedef struct {
    /* called once on attach; backend may allocate state into *bstate. */
    int  (*init)(void **bstate, int cols, int rows);
    void (*destroy)(void *bstate);
    /* clear + draw one line of (text, len) at view row `row` (0-based).
     * `kind` is a LexTok (0 = plain) -> colour/style hint. */
    void (*draw_line)(void *bstate, int row, const char *text, int len, int kind);
    /* draw the caret at view (row, col). */
    void (*draw_caret)(void *bstate, int row, int col);
    /* refresh after a batch of draws. */
    void (*present)(void *bstate);
    /* block for one key event; fill *ch (utf8 bytes), set *key to a
     * symbolic code (see UI_KEY_*), return 0 on success, -1 to quit. */
    int  (*get_key)(void *bstate, char *ch, int *key);
    /* resize notification; cols/rows updated before next draw. */
    void (*resize)(void *bstate, int *cols, int *rows);
    /* --- optional chrome (Phase C); NULL = backend doesn't draw it --- */
    /* rows reserved above+below the text viewport (tab strip + status). */
    int  (*chrome_rows)(void *bstate);
    /* draw a line-number gutter cell for view row `row` (1-based line_no). */
    void (*draw_gutter)(void *bstate, int row, int line_no);
    /* draw the top tab strip: one cell per open doc; active/dirty flagged. */
    void (*draw_tab)(void *bstate, int tab_row, int index,
                     const char *name, int active, int dirty);
    /* draw the bottom status line. */
    void (*draw_status)(void *bstate, int row, const char *text);
    /* apply a theme variant (1=dark, 0=light). */
    void (*set_theme)(void *bstate, int dark);
} UI_Backend;

/* Symbolic keys (when *key is set, *ch may still carry printable bytes). */
enum {
    UI_KEY_NONE = 0,
    UI_KEY_QUIT,        /* ctrl-q or backend quit */
    UI_KEY_LEFT, UI_KEY_RIGHT, UI_KEY_UP, UI_KEY_DOWN,
    UI_KEY_HOME, UI_KEY_END, UI_KEY_PGUP, UI_KEY_PGDOWN,
    UI_KEY_BACKSPACE, UI_KEY_ENTER, UI_KEY_DEL,
    UI_KEY_UNDO, UI_KEY_REDO,
    UI_KEY_FIND,
    UI_KEY_NEXTTAB, UI_KEY_PREVTAB,
    UI_KEY_THEME,
    UI_KEY_COLMODE,      /* toggle column/block selection */
    UI_KEY_EOL,          /* convert EOL (LF<->CRLF) */
    UI_KEY_MACRO,        /* toggle macro recording */
    UI_KEY_REPLAY        /* replay last macro */
};

/* Create a UI bound to `doc` and backend `be`. cols/rows seed the viewport. */
UI *ui_create(Doc *doc, const UI_Backend *be, int cols, int rows);
void ui_free(UI *ui);

/* Viewport size (backend may resize via ui_resize). */
void ui_resize(UI *ui, int cols, int rows);
void ui_get_size(const UI *ui, int *cols, int *rows);

/* --- editing commands (all funnel through Doc; undoable) --- */
void ui_insert_text(UI *ui, const char *text, size_t len);
void ui_backspace(UI *ui);
void ui_delete(UI *ui);
void ui_newline(UI *ui);
void ui_cursor_left(UI *ui);
void ui_cursor_right(UI *ui);
void ui_cursor_up(UI *ui);
void ui_cursor_down(UI *ui);
void ui_cursor_home(UI *ui);
void ui_cursor_end(UI *ui);
void ui_page_up(UI *ui);
void ui_page_down(UI *ui);
void ui_undo(UI *ui);
void ui_redo(UI *ui);

/* Scroll the viewport to ensure the cursor is visible (call after moves). */
void ui_scroll_to_cursor(UI *ui);

/* --- find / replace (bind the DONE search engine; Phase B) --- */
/* Run a find over the doc; moves the selection to the first match. Returns
 * the match count (0 none, -1 bad regex). */
long ui_find(UI *ui, const char *pattern, int regex, int icase);
/* Advance to the next match (wraps). */
void ui_find_next_match(UI *ui);
/* Replace every match in the doc with `repl`. Returns count replaced. */
long ui_find_replace_all_in_doc(UI *ui, const char *repl);
/* Read-only accessors for the active find state (for status display). */
long ui_find_matches(const UI *ui);
int  ui_find_error(const UI *ui);

/* --- theme (Phase C) --- */
void ui_set_theme(UI *ui, int dark);    /* 1=dark, 0=light */
void ui_toggle_theme(UI *ui);           /* flip + persist */
int  ui_theme_dark(const UI *ui);       /* 1=dark, 0=light */

/* --- multi-doc session (Phase C) --- */
/* Attach a Docs session so the UI shows a tab strip + switches the active
 * document on tab activation. Pass NULL to detach (single-doc mode). */
void ui_set_docs(UI *ui, Docs *docs);
/* switch active document (Ctrl-T / Ctrl-Shift-T). */
void ui_next_tab(UI *ui);
void ui_prev_tab(UI *ui);

/* --- macro (Phase D) --- */
/* Attach a macro recorder; UI_KEY_MACRO toggles recording, UI_KEY_REPLAY
 * replays. May be NULL (no macro support). */
void ui_set_macro(UI *ui, UIMacro *m);
void ui_toggle_macro(UI *ui);   /* start/stop recording */
void ui_replay_macro(UI *ui);   /* replay last recording */

/* --- column / EOL (Phase D) --- */
int  ui_get_colmode(const UI *ui);
void ui_toggle_colmode(UI *ui);
void ui_convert_eol(UI *ui);    /* flip LF <-> CRLF */

/* Status string buffer builder (cursor Ln/Col, lang, dirty + find count). */
void ui_status_string(const UI *ui, const char *lang, char *buf, size_t n);

/* --- render --- */
/* Redraw the visible viewport. `lang` selects the lexer for highlighting
 * (NULL = plain). Draws cols*rows lines starting at the scroll row. */
void ui_render(UI *ui, const char *lang);

/* --- main loop --- */
/* Run an interactive editing session until the backend signals quit.
 * `lang` drives syntax highlighting. Returns 0 on clean exit. */
int ui_run(UI *ui, const char *lang);

/* Apply one input event (dispatch + scroll, no backend read, no render).
 * Used by ui_step and by macro replay. Records into the macro if attached. */
void ui_apply(UI *ui, char ch, int key);

/* Advance one input event: read a key, dispatch it, scroll, render.
 * Returns 0 to continue, -1 if quit was requested. Used by tests and by
 * front-ends that own their own event pump. */
int ui_step(UI *ui, const char *lang);

#endif /* WUBUPAD_UI_H */
