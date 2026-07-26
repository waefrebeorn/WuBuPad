/* ui_find.h -- find/replace controller over the document core.
 *
 * Backend-agnostic: collects match ranges via the DONE search engine
 * (regex_find_from / search_literal) and drives the Doc selection. The visual
 * find bar is drawn by the backend (TTY prompt / gfx panel) in Phase C; this
 * module owns the logic + match navigation + replace. Clean C11, opaque. */
#ifndef WUBUPAD_UI_FIND_H
#define WUBUPAD_UI_FIND_H

#include <stddef.h>

typedef struct UIFind UIFind;

/* Create a find controller. */
UIFind *ui_find_create(void);
void ui_find_free(UIFind *f);

/* Run a search over `text` (doc_text result). `pattern` may be empty to clear.
 * `regex` enables regex mode; `icase` enables case-insensitivity.
 * Returns the number of matches found (0 if none / bad pattern), or -1 on a
 * malformed regex (pattern retained for the bar to show an error). */
long ui_find_run(UIFind *f, const char *text, size_t len,
                 const char *pattern, int regex, int icase);

/* Current query + flags (for the bar display). */
const char *ui_find_pattern(const UIFind *f);
int ui_find_is_regex(const UIFind *f);
int ui_find_is_icase(const UIFind *f);
int ui_find_bad_pattern(const UIFind *f);   /* 1 if last run had a bad regex */
long ui_find_count(const UIFind *f);         /* total matches */

/* Navigation: returns the byte range [start,end) of the active match and
 * updates the active index (wraps). `dir` = +1 next, -1 prev. Returns 1 if a
 * match is active, 0 if none. */
int ui_find_active(const UIFind *f, size_t *start, size_t *end);
int ui_find_next(UIFind *f);
int ui_find_prev(UIFind *f);

/* Replace the active match in `text` (caller passes current doc_text) with
 * `repl`; returns new length via *newlen and new text via realloc'd *out
 * (caller frees), or returns -1 if no active match. The active index advances
 * to the next match. */
int ui_find_replace(const UIFind *f, const char *text, size_t len,
                    const char *repl, char **out, size_t *newlen);

/* Replace ALL matches. Returns count replaced; out/newlen as above. */
long ui_find_replace_all(const UIFind *f, const char *text, size_t len,
                         const char *repl, char **out, size_t *newlen);

#endif /* WUBUPAD_UI_FIND_H */
