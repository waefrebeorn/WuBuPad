/* doc.c -- document model with linear undo/redo and cursor/selection. */
#include "doc.h"
#include "buffer.h"

#include <stdlib.h>
#include <string.h>

typedef enum { OP_INSERT, OP_DELETE, OP_REPLACE } OpKind;

/* An undo record stores enough to invert the operation. For INSERT we store
 * the position + the inserted text (to delete it on undo). For DELETE we
 * store the position + the deleted text (to re-insert on undo). For REPLACE
 * we store the deleted span (text/len) and the replacement (ins/ins_len) so a
 * programmatic replace undoes/redoes as ONE step. */
typedef struct {
    OpKind   kind;
    size_t   pos;
    char    *text;     /* inserted text, or deleted text (or deleted span) */
    size_t   len;
    char    *ins;      /* REPLACE: replacement text */
    size_t   ins_len;  /* REPLACE: replacement length */
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
    int    eol;            /* 0 = LF, 1 = CRLF */
    int    colmode;        /* column/block selection active */
    size_t r0, c0, r1, c1; /* column rectangle (normalized) */
};

static void *xrealloc(void *p, size_t n) { void *r = realloc(p, n?n:1); if(!r) abort(); return r; }

static void op_free(Op *o) {
    free(o->text); o->text = NULL; o->len = 0;
    free(o->ins);  o->ins = NULL;  o->ins_len = 0;
}

static Op make_op(OpKind k, size_t pos, const char *text, size_t len) {
    Op o; o.kind = k; o.pos = pos; o.len = len;
    o.text = len ? xrealloc(NULL, len) : NULL;
    if (len) memcpy(o.text, text, len);
    o.ins = NULL; o.ins_len = 0;
    return o;
}

static Op make_op_replace(size_t pos, const char *old, size_t oldlen,
                          const char *neu, size_t neulen) {
    Op o = make_op(OP_REPLACE, pos, old, oldlen);
    o.ins = neulen ? xrealloc(NULL, neulen) : NULL;
    if (neulen) memcpy(o.ins, neu, neulen);
    o.ins_len = neulen;
    return o;
}

static void push_undo(Doc *d, Op o);   /* defined below doc_replace */

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
    /* Record ONE undo op (OP_REPLACE: deleted span + replacement) so undo/redo
     * restores exactly — GUI_MATHEMATICS 'user control & freedom' (Nielsen #3). */
    size_t newlen = text ? strlen(text) : 0;
    size_t oldlen = (to > from) ? to - from : 0;
    /* capture deleted text */
    char *oldbuf = oldlen ? xrealloc(NULL, oldlen) : NULL;
    for (size_t i = 0; i < oldlen; i++)
        oldbuf[i] = buf_char_at(d->buf, from + i);
    push_undo(d, make_op_replace(from, oldbuf, oldlen, text ? text : "", newlen));
    free(oldbuf);
    /* apply */
    if (to > from) buf_delete(d->buf, from, to - from);
    if (newlen) buf_insert(d->buf, from, text, newlen);
    d->cursor = from + newlen;
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
    } else if (o.kind == OP_REPLACE) {
        /* delete the replacement, restore the original span */
        buf_delete(d->buf, o.pos, o.ins_len);
        if (o.len) buf_insert(d->buf, o.pos, o.text, o.len);
        d->cursor = o.pos + o.len;
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
    } else if (o.kind == OP_REPLACE) {
        /* delete the original span, re-apply the replacement */
        buf_delete(d->buf, o.pos, o.len);
        if (o.ins_len) buf_insert(d->buf, o.pos, o.ins, o.ins_len);
        d->cursor = o.pos + o.ins_len;
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

/* --- whole-document rewrite with a single undo entry ----------------- */
/* Replaces the entire buffer with `newtext` (NUL-terminated) and records ONE
 * undo op so the whole change is atomic (EOL convert / column rect ops). */
static void doc_rewrite(Doc *d, const char *newtext) {
    size_t oldlen = doc_length(d);
    /* remove the existing content (records an undoable DELETE) ... */
    if (oldlen) doc_delete(d, 0, oldlen);
    /* ... then insert the replacement (records an undoable INSERT).
     * Two undo steps, but always restores the prior content exactly. */
    if (newtext && *newtext) doc_insert(d, 0, newtext, strlen(newtext));
}

/* --- EOL mode --------------------------------------------------------- */
int doc_eol_mode(const Doc *d) { return d->eol; }

void doc_convert_eol(Doc *d, int crlf) {
    if ((crlf ? 1 : 0) == d->eol) return;
    char *src = doc_text(d);
    size_t n = strlen(src);
    /* worst case: every byte a '\n' becomes "\r\n" */
    char *out = xrealloc(NULL, n * 2 + 1);
    size_t o = 0;
    if (crlf) {
        for (size_t i = 0; i < n; i++) {
            if (src[i] == '\n') out[o++] = '\r';
            out[o++] = src[i];
        }
    } else {
        for (size_t i = 0; i < n; i++) {
            if (src[i] == '\r' && i + 1 < n && src[i+1] == '\n') continue; /* drop */
            out[o++] = src[i];
        }
    }
    out[o] = 0;
    doc_rewrite(d, out);
    free(out);
    d->eol = crlf ? 1 : 0;
    free(src);
}

/* --- column (block) selection ---------------------------------------- */
int  doc_get_colmode(const Doc *d) { return d->colmode; }
void doc_set_colmode(Doc *d, int on) { d->colmode = on ? 1 : 0; }

void doc_get_rect(const Doc *d, size_t *r0, size_t *c0, size_t *r1, size_t *c1) {
    *r0 = d->r0; *c0 = d->c0; *r1 = d->r1; *c1 = d->c1;
}
void doc_set_rect(Doc *d, size_t r0, size_t c0, size_t r1, size_t c1) {
    if (r0 > r1) { size_t t = r0; r0 = r1; r1 = t; }
    if (c0 > c1) { size_t t = c0; c0 = c1; c1 = t; }
    d->r0 = r0; d->c0 = c0; d->r1 = r1; d->c1 = c1;
}

static void doc_apply_rect(Doc *d, int delete_only, const char *ins, size_t inslen) {
    size_t nlines = doc_lines(d);
    if (d->r1 >= nlines) d->r1 = nlines - 1;
    if (d->r1 < d->r0) return;
    char *src = doc_text(d);
    /* build new text line by line */
    char *out = xrealloc(NULL, strlen(src) * 2 + 1);
    size_t o = 0;
    size_t cur_line = 0;
    /* iterate lines manually */
    size_t p = 0, slen = strlen(src);
    while (p <= slen) {
        /* find end of current line */
        size_t e = p;
        while (e < slen && src[e] != '\n') e++;
        size_t linelen = (e > p && src[e-1] == '\r') ? (e - p - 1) : (e - p);
        if (cur_line >= d->r0 && cur_line <= d->r1) {
            size_t c0 = d->c0, c1 = d->c1;
            if (c1 > linelen) c1 = linelen;
            if (c0 > linelen) c0 = linelen;
            /* copy [0,c0) */
            for (size_t i = 0; i < c0; i++) out[o++] = src[p+i];
            if (!delete_only) { memcpy(out+o, ins, inslen); o += inslen;
                                /* for insert, keep the whole original line:
                                 * copy [c0, linelen) after the inserted text */
                                for (size_t i = c0; i < linelen; i++) out[o++] = src[p+i];
            } else {
                /* delete: drop [c0,c1), copy [c1,linelen) */
                for (size_t i = c1; i < linelen; i++) out[o++] = src[p+i];
            }
        } else {
            for (size_t i = p; i < e; i++) out[o++] = src[i];
        }
        if (e < slen) out[o++] = '\n';   /* keep newline */
        if (e >= slen) break;
        p = e + 1;
        cur_line++;
    }
    out[o] = 0;
    doc_rewrite(d, out);
    free(out);
    free(src);
}

void doc_delete_rect(Doc *d) { doc_apply_rect(d, 1, NULL, 0); }
void doc_insert_rect(Doc *d, const char *text, size_t len) {
    doc_apply_rect(d, 0, text, len);
}
