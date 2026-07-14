/* doc.h -- document model: owns a Buf, an undo stack, and a cursor/selection.
 * Opaque. Edits funnel through document_apply() which records inverse ops so
 * undo/redo is exact. Clean C11. */
#ifndef WUBUPAD_DOC_H
#define WUBUPAD_DOC_H

#include <stddef.h>

typedef struct Doc Doc;

Doc *doc_create(const char *text);
void doc_free(Doc *d);

/* current document length (bytes) */
size_t doc_length(const Doc *d);
/* collect document into malloc'd NUL-terminated string (caller frees) */
char *doc_text(const Doc *d);
/* line count */
size_t doc_lines(const Doc *d);

/* ---- editing (records undo) ----------------------------------------- */
void doc_insert(Doc *d, size_t pos, const char *text, size_t len);
void doc_delete(Doc *d, size_t pos, size_t len);

/* undo/redo availability + actions */
int  doc_can_undo(const Doc *d);
int  doc_can_redo(const Doc *d);
void doc_undo(Doc *d);
void doc_redo(Doc *d);

/* ---- cursor / selection --------------------------------------------- */
size_t doc_cursor(const Doc *d);
void   doc_set_cursor(Doc *d, size_t pos);
size_t doc_sel_start(const Doc *d);
size_t doc_sel_end(const Doc *d);
void   doc_set_selection(Doc *d, size_t anchor, size_t cursor);
void   doc_clear_selection(Doc *d);
int    doc_has_selection(const Doc *d);

/* Convenience: insert at cursor (replacing selection if any), move cursor. */
void doc_type(Doc *d, const char *text, size_t len);

#endif /* WUBUPAD_DOC_H */
