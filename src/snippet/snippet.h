/* snippet.h -- snippets engine (Atom "snippets" package).
 *
 * A snippet is a template with ${1:default} tabstops and $1 mirror fields.
 * Expansion places the text, records tabstop ranges, and lets the user Tab
 * between them (typing replaces the placeholder). Mirrors (a second $1)
 * mirror the first tabstop's text live. Operates on a supplied text buffer
 * abstraction (the host Doc), so the engine is headless-testable. Opaque. */
#ifndef WUBUPAD_SNIPPET_H
#define WUBUPAD_SNIPPET_H

#include <stddef.h>

typedef struct SnippetEngine SnippetEngine;

/* Host buffer callbacks: insert `text`(len) at `pos`, delete [from,to). */
typedef void (*snip_insert_fn)(void *buf, size_t pos, const char *text, size_t len);
typedef void (*snip_delete_fn)(void *buf, size_t from, size_t to);
typedef size_t (*snip_len_fn)(void *buf);

SnippetEngine *snippet_create(void);
void snippet_free(SnippetEngine *e);

/* Register a named snippet bound to a trigger (e.g. "for" -> "for(...)...").
 * Returns 0 on success. */
int snippet_add(SnippetEngine *e, const char *trigger, const char *body);

/* Expand the snippet for `trigger` at buffer position `pos`. Records
 * tabstops in the engine state. Returns the number of tabstops (0 = no
 * snippet / nothing inserted), or -1 on error. The caller should position
 * the cursor at the first tabstop start. */
int snippet_expand(SnippetEngine *e, void *buf, snip_insert_fn ins,
                   snip_delete_fn del, snip_len_fn lenfn,
                   const char *trigger, size_t pos);

/* Advance to the next tabstop (Tab). Fills *out_from/*out_to with the active
 * placeholder range (to overwrite). Returns 1 if a tabstop is active, 0 if
 * none (expansion finished). Mirrors are auto-synced by the host on edit. */
int snippet_next(SnippetEngine *e, size_t *out_from, size_t *out_to);

/* Active (currently-selected) placeholder range, or 0,0 if none. */
void snippet_active(SnippetEngine *e, size_t *out_from, size_t *out_to);

/* True while an expansion is in progress (Tab cycles placeholders). */
int snippet_active_p(const SnippetEngine *e);

#endif /* WUBUPAD_SNIPPET_H */
