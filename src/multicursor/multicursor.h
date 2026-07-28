/* multicursor.h -- multiple cursors (Atom "multiple-cursors" package).
 *
 * A set of carets, each with an optional selection anchor. Operations apply
 * to every cursor in parallel against a host buffer via callbacks, so the
 * engine is headless-testable and frontend-agnostic. Opaque, C11. */
#ifndef WUBUPAD_MULTICURSOR_H
#define WUBUPAD_MULTICURSOR_H

#include <stddef.h>

typedef struct MultiCursor MultiCursor;

/* host ops: insert text at pos; set cursor/selection on the host Doc. */
typedef void (*mc_insert_fn)(void *buf, size_t pos, const char *text, size_t len);
typedef void (*mc_set_fn)(void *buf, size_t cursor, size_t anchor);

MultiCursor *multicursor_create(void);
void multicursor_free(MultiCursor *m);

/* Add a caret (with optional selection anchor == cursor for none). */
void multicursor_add(MultiCursor *m, size_t cursor, size_t anchor);

/* Clear all carets. */
void multicursor_clear(MultiCursor *m);

/* Number of carets. */
size_t multicursor_count(const MultiCursor *m);

/* Insert `text` at every caret (parallel edit). Each cursor's offset is
 * adjusted for prior inserts. `set` reports the new cursor/anchor per caret. */
void multicursor_insert_all(MultiCursor *m, void *buf, mc_insert_fn ins,
                            mc_set_fn set, const char *text, size_t len);

/* Cursor positions (sorted). out must hold at least multicursor_count() slots. */
void multicursor_cursors(const MultiCursor *m, size_t *out);

#endif /* WUBUPAD_MULTICURSOR_H */
