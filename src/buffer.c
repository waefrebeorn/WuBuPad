/* buffer.c -- piece-table implementation. See buffer.h. */
#include "buffer.h"

#include <stdlib.h>
#include <string.h>

typedef enum { SRC_ORIG, SRC_ADD } PieceSrc;

typedef struct {
    PieceSrc src;     /* which backing store the piece references */
    size_t   off;     /* offset into that store */
    size_t   len;     /* length of this piece */
} Piece;

struct Buf {
    char  *orig;          /* immutable original text */
    size_t orig_len;
    char  *add;           /* append-only insert buffer */
    size_t add_len;
    size_t add_cap;
    Piece *pieces;        /* array of pieces (logical order) */
    size_t n;             /* number of pieces */
    size_t cap;           /* capacity of pieces array */
};

static void *xrealloc(void *p, size_t n) {
    void *r = realloc(p, n ? n : 1);
    if (!r) abort();
    return r;
}

static void pieces_push(Buf *b, Piece p) {
    if (b->n == b->cap) {
        b->cap = b->cap ? b->cap * 2 : 8;
        b->pieces = xrealloc(b->pieces, b->cap * sizeof *b->pieces);
    }
    b->pieces[b->n++] = p;
}

static void add_append(Buf *b, const char *text, size_t len) {
    if (b->add_len + len + 1 > b->add_cap) {
        while (b->add_len + len + 1 > b->add_cap)
            b->add_cap = b->add_cap ? b->add_cap * 2 : 64;
        b->add = xrealloc(b->add, b->add_cap);
    }
    memcpy(b->add + b->add_len, text, len);
    b->add_len += len;
}

Buf *buf_create(const char *text) {
    Buf *b = xrealloc(NULL, sizeof *b);
    memset(b, 0, sizeof *b);
    size_t n = text ? strlen(text) : 0;
    if (n) {
        b->orig = xrealloc(NULL, n + 1);
        memcpy(b->orig, text, n);
        b->orig[n] = '\0';
        b->orig_len = n;
        pieces_push(b, (Piece){ SRC_ORIG, 0, n });
    }
    return b;
}

void buf_free(Buf *b) {
    if (!b) return;
    free(b->orig);
    free(b->add);
    free(b->pieces);
    free(b);
}

static const char *piece_base(const Buf *b, const Piece *p) {
    return p->src == SRC_ORIG ? b->orig : b->add;
}

size_t buf_length(const Buf *b) {
    size_t total = 0;
    for (size_t i = 0; i < b->n; i++) total += b->pieces[i].len;
    return total;
}

char buf_char_at(const Buf *b, size_t pos) {
    size_t acc = 0;
    for (size_t i = 0; i < b->n; i++) {
        const Piece *p = &b->pieces[i];
        if (pos < acc + p->len)
            return piece_base(b, p)[p->off + (pos - acc)];
        acc += p->len;
    }
    return 0;
}

size_t buf_collect(const Buf *b, char *out) {
    size_t acc = 0;
    for (size_t i = 0; i < b->n; i++) {
        const Piece *p = &b->pieces[i];
        memcpy(out + acc, piece_base(b, p) + p->off, p->len);
        acc += p->len;
    }
    out[acc] = '\0';
    return acc;
}

char *buf_to_string(const Buf *b) {
    size_t len = buf_length(b);
    char *s = xrealloc(NULL, len + 1);
    buf_collect(b, s);
    return s;
}

size_t buf_line_count(const Buf *b) {
    size_t lines = 1, total = buf_length(b);
    for (size_t i = 0; i < total; i++)
        if (buf_char_at(b, i) == '\n') lines++;
    return lines;
}

/* Locate the piece containing document position `pos` and the in-piece
 * offset; returns piece index, or b->n (sentinel) if pos == end. */
static size_t find_piece(const Buf *b, size_t pos, size_t *in_off) {
    size_t acc = 0;
    for (size_t i = 0; i < b->n; i++) {
        size_t plen = b->pieces[i].len;
        if (pos <= acc + plen) {          /* <= so end-of-doc lands here */
            *in_off = pos - acc;
            return i;
        }
        acc += plen;
    }
    *in_off = 0;
    return b->n;
}

void buf_insert(Buf *b, size_t pos, const char *text, size_t len) {
    if (len == 0) return;
    if (pos > buf_length(b)) pos = buf_length(b);

    size_t in_off, idx = find_piece(b, pos, &in_off);

    /* appending at the very end: just add a new piece onto ADD. */
    if (idx == b->n) {
        add_append(b, text, len);
        pieces_push(b, (Piece){ SRC_ADD, b->add_len - len, len });
        return;
    }

    const Piece *p = &b->pieces[idx];
    if (in_off == 0) {
        /* insert before piece `idx`: new ADD piece, then existing piece. */
        add_append(b, text, len);
        Piece np = { SRC_ADD, b->add_len - len, len };
        /* shift to make room at idx */
        if (b->n + 1 > b->cap) { b->cap = b->cap ? b->cap*2 : 8; b->pieces = xrealloc(b->pieces, b->cap*sizeof *b->pieces); }
        memmove(&b->pieces[idx+1], &b->pieces[idx], (b->n - idx) * sizeof *b->pieces);
        b->pieces[idx] = np;
        b->n++;
        return;
    }
    if (in_off == p->len) {
        /* insert after piece `idx`: new piece after it. */
        add_append(b, text, len);
        Piece np = { SRC_ADD, b->add_len - len, len };
        if (b->n + 1 > b->cap) { b->cap = b->cap*2; b->pieces = xrealloc(b->pieces, b->cap*sizeof *b->pieces); }
        memmove(&b->pieces[idx+2], &b->pieces[idx+1], (b->n - (idx+1)) * sizeof *b->pieces);
        b->pieces[idx+1] = np;
        b->n++;
        return;
    }
    /* split piece `idx` at in_off into [0,in_off) + new + (in_off,end). */
    add_append(b, text, len);
    Piece left  = { p->src, p->off, in_off };
    Piece ins   = { SRC_ADD, b->add_len - len, len };
    Piece right = { p->src, p->off + in_off, p->len - in_off };
    if (b->n + 2 > b->cap) { b->cap = (b->n+2)*2; b->pieces = xrealloc(b->pieces, b->cap*sizeof *b->pieces); }
    memmove(&b->pieces[idx+3], &b->pieces[idx+1], (b->n - (idx+1)) * sizeof *b->pieces);
    b->pieces[idx]   = left;
    b->pieces[idx+1] = ins;
    b->pieces[idx+2] = right;
    b->n += 2;
}

/* Ensure a piece boundary exists exactly at document position `pos`.
 * Returns the index of the piece that now STARTS at `pos` (or b->n if pos is
 * the very end). Existing content is preserved; no bytes are moved. */
static size_t split_at(Buf *b, size_t pos) {
    if (pos == 0) return 0;
    if (pos >= buf_length(b)) return b->n;
    size_t in_off, idx = find_piece(b, pos, &in_off);
    if (in_off == 0) return idx;                 /* already a boundary */
    const Piece *p = &b->pieces[idx];
    if (in_off == p->len) return idx + 1;        /* boundary is piece end */
    /* split piece idx into [0,in_off) and (in_off,end) */
    Piece left  = { p->src, p->off, in_off };
    Piece right = { p->src, p->off + in_off, p->len - in_off };
    if (b->n + 1 > b->cap) { b->cap = b->cap ? b->cap*2 : 8; b->pieces = xrealloc(b->pieces, b->cap*sizeof *b->pieces); }
    memmove(&b->pieces[idx+1], &b->pieces[idx], (b->n - idx) * sizeof *b->pieces);
    b->pieces[idx]   = left;
    b->pieces[idx+1] = right;
    b->n++;
    return idx + 1;   /* piece starting at pos */
}

void buf_delete(Buf *b, size_t pos, size_t len) {
    size_t total = buf_length(b);
    if (pos >= total || len == 0) return;
    if (pos + len > total) len = total - pos;
    if (len == 0) return;

    size_t end = pos + len;
    /* Make clean boundaries at both ends; the gap between is a whole set of
     * pieces we can drop without any truncation. */
    size_t i0 = split_at(b, pos);
    size_t i1 = split_at(b, end);
    if (i1 > i0) {
        memmove(&b->pieces[i0], &b->pieces[i1], (b->n - i1) * sizeof *b->pieces);
        b->n -= (i1 - i0);
    }
}
