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
/* map a byte position to 0-based (line, col) */
void doc_line_col(const Doc *d, size_t pos, size_t *line, size_t *col);
/* byte offset of the start of line `line` (0-based) */
size_t doc_line_byte_start(const Doc *d, size_t line);
/* total byte length of the document */
size_t doc_length(const Doc *d);

/* replace the range [from,to) with `text` (records undo as delete+insert).
 * Convenience for search/replace driven by the agent protocol. */
void doc_replace(Doc *d, size_t from, size_t to, const char *text);

/* ---- editing (records undo) ----------------------------------------- */
void doc_insert(Doc *d, size_t pos, const char *text, size_t len);
void doc_delete(Doc *d, size_t pos, size_t len);

/* ---- EOL mode -------------------------------------------------------- */
/* 0 = LF (Unix), 1 = CRLF (Windows). */
int  doc_eol_mode(const Doc *d);
/* Rewrite the whole document to the requested EOL mode (records one undo). */
void doc_convert_eol(Doc *d, int crlf);

/* ---- column (block) selection --------------------------------------- */
/* When column mode is on, the selection is a rectangle (line/col) rather than
 * a linear span. rect is normalized: r0<=r1, c0<=c1. */
int  doc_get_colmode(const Doc *d);
void doc_set_colmode(Doc *d, int on);
void doc_get_rect(const Doc *d, size_t *r0, size_t *c0, size_t *r1, size_t *c1);
void doc_set_rect(Doc *d, size_t r0, size_t c0, size_t r1, size_t c1);
/* delete / insert across the current column rectangle (per line). */
void doc_delete_rect(Doc *d);
void doc_insert_rect(Doc *d, const char *text, size_t len);

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
