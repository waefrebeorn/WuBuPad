/* doc.c -- document model with linear undo/redo and cursor/selection. */
#include "doc.h"
#include "buffer.h"

#include <stdlib.h>
#include <string.h>

typedef enum { OP_INSERT, OP_DELETE } OpKind;

/* An undo record stores enough to invert the operation. For INSERT we store
 * the position + the inserted text (to delete it on undo). For DELETE we
 * store the position + the deleted text (to re-insert on undo). */
typedef struct {
    OpKind   kind;
    size_t   pos;
    char    *text;     /* inserted text, or deleted text */
    size_t   len;
} Op;

#define UNDO_CAP 64

struct Doc {
    Buf   *buf;
    Op     undo[UNDO_CAP];
    int    undo_top;       /* -1 = empty; index of last valid */
    Op     redo[UNDO_CAP];
    int    redo_top;
    size_t cursor;         /* current caret position */
    size_t anchor;         /* selection anchor (== cursor if no selection) */
};

static void *xrealloc(void *p, size_t n) { void *r = realloc(p, n?n:1); if(!r) abort(); return r; }

static void op_free(Op *o) { free(o->text); o->text = NULL; o->len = 0; }

static Op make_op(OpKind k, size_t pos, const char *text, size_t len) {
    Op o; o.kind = k; o.pos = pos; o.len = len;
    o.text = len ? xrealloc(NULL, len) : NULL;
    if (len) memcpy(o.text, text, len);
    return o;
}

Doc *doc_create(const char *text) {
    Doc *d = xrealloc(NULL, sizeof *d);
    memset(d, 0, sizeof *d);
    d->buf = buf_create(text);
    d->undo_top = -1;
    d->redo_top = -1;
    d->cursor = doc_length(d);
    d->anchor = d->cursor;
    return d;
}

void doc_free(Doc *d) {
    if (!d) return;
    for (int i = 0; i <= d->undo_top; i++) op_free(&d->undo[i]);
    for (int i = 0; i <= d->redo_top; i++) op_free(&d->redo[i]);
    buf_free(d->buf);
    free(d);
}

size_t doc_length(const Doc *d) { return buf_length(d->buf); }
char  *doc_text(const Doc *d) { return buf_to_string(d->buf); }
size_t doc_lines(const Doc *d) { return buf_line_count(d->buf); }
void doc_line_col(const Doc *d, size_t pos, size_t *line, size_t *col) {
    buf_line_col_of(d->buf, pos, line, col);
}
size_t doc_line_byte_start(const Doc *d, size_t line) {
    return buf_line_start(d->buf, line);
}
void doc_replace(Doc *d, size_t from, size_t to, const char *text) {
    if (to > from) buf_delete(d->buf, from, to - from);
    if (text && *text) buf_insert(d->buf, from, text, strlen(text));
    d->cursor = from + (text ? strlen(text) : 0);
    d->anchor = d->cursor;
}

static void push_undo(Doc *d, Op o) {
    /* drop any redo history on a new edit */
    for (int i = 0; i <= d->redo_top; i++) op_free(&d->redo[i]);
    d->redo_top = -1;
    if (d->undo_top + 1 >= UNDO_CAP) {
        /* evict oldest (shift down) */
        op_free(&d->undo[0]);
        memmove(&d->undo[0], &d->undo[1], (UNDO_CAP - 1) * sizeof(Op));
        d->undo_top--;
    }
    d->undo[++d->undo_top] = o;
}

void doc_insert(Doc *d, size_t pos, const char *text, size_t len) {
    if (len == 0) return;
    if (pos > doc_length(d)) pos = doc_length(d);
    buf_insert(d->buf, pos, text, len);
    push_undo(d, make_op(OP_INSERT, pos, text, len));
    if (pos <= d->cursor) d->cursor += len;
    d->anchor = d->cursor;
}

void doc_delete(Doc *d, size_t pos, size_t len) {
    size_t total = doc_length(d);
    if (pos >= total || len == 0) return;
    if (pos + len > total) len = total - pos;
    /* capture the deleted text for redo */
    char *buf = xrealloc(NULL, len);
    /* read it out before deleting */
    {
        char *full = doc_text(d);
        memcpy(buf, full + pos, len);
        free(full);
    }
    buf_delete(d->buf, pos, len);
    push_undo(d, make_op(OP_DELETE, pos, buf, len));
    free(buf);
    if (pos < d->cursor) {
        size_t moved = (pos + len <= d->cursor) ? len : (d->cursor - pos);
        d->cursor -= moved;
    }
    d->anchor = d->cursor;
}

int doc_can_undo(const Doc *d) { return d->undo_top >= 0; }
int doc_can_redo(const Doc *d) { return d->redo_top >= 0; }

void doc_undo(Doc *d) {
    if (!doc_can_undo(d)) return;
    Op o = d->undo[d->undo_top--];
    if (o.kind == OP_INSERT) {
        buf_delete(d->buf, o.pos, o.len);
        if (o.pos <= d->cursor) d->cursor -= o.len;
    } else { /* DELETE: re-insert */
        buf_insert(d->buf, o.pos, o.text, o.len);
        if (o.pos <= d->cursor) d->cursor += o.len;
    }
    d->anchor = d->cursor;
    /* move to redo */
    if (d->redo_top + 1 >= UNDO_CAP) {
        op_free(&d->redo[0]);
        memmove(&d->redo[0], &d->redo[1], (UNDO_CAP-1)*sizeof(Op));
        d->redo_top--;
    }
    d->redo[++d->redo_top] = o;
}

void doc_redo(Doc *d) {
    if (!doc_can_redo(d)) return;
    Op o = d->redo[d->redo_top--];
    if (o.kind == OP_INSERT) {
        buf_insert(d->buf, o.pos, o.text, o.len);
        if (o.pos <= d->cursor) d->cursor += o.len;
    } else { /* DELETE: delete again */
        buf_delete(d->buf, o.pos, o.len);
        if (o.pos <= d->cursor) d->cursor -= o.len;
    }
    d->anchor = d->cursor;
    if (d->undo_top + 1 >= UNDO_CAP) {
        op_free(&d->undo[0]);
        memmove(&d->undo[0], &d->undo[1], (UNDO_CAP-1)*sizeof(Op));
        d->undo_top--;
    }
    d->undo[++d->undo_top] = o;
}

size_t doc_cursor(const Doc *d) { return d->cursor; }
void   doc_set_cursor(Doc *d, size_t pos) {
    if (pos > doc_length(d)) pos = doc_length(d);
    d->cursor = pos; d->anchor = pos;
}
size_t doc_sel_start(const Doc *d) { return d->cursor < d->anchor ? d->cursor : d->anchor; }
size_t doc_sel_end(const Doc *d)   { return d->cursor < d->anchor ? d->anchor : d->cursor; }
void   doc_set_selection(Doc *d, size_t anchor, size_t cursor) {
    if (anchor > doc_length(d)) anchor = doc_length(d);
    if (cursor > doc_length(d)) cursor = doc_length(d);
    d->anchor = anchor; d->cursor = cursor;
}
void doc_clear_selection(Doc *d) { d->anchor = d->cursor; }
int  doc_has_selection(const Doc *d) { return d->cursor != d->anchor; }

void doc_type(Doc *d, const char *text, size_t len) {
    if (doc_has_selection(d)) {
        size_t s = doc_sel_start(d), e = doc_sel_end(d);
        doc_delete(d, s, e - s);
        doc_insert(d, s, text, len);
    } else {
        doc_insert(d, d->cursor, text, len);
    }
}
