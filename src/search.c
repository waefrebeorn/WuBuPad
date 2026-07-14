/* search.c -- literal search + Thompson NFA regex. Self-contained C11.
 *
 * Thompson NFA construction (Russ Cox): each State has an optional character
 * class and two out-edges (out, out1). A "Split"/epsilon state has an empty
 * class and follows both edges without consuming input; a literal state
 * consumes one character. Fragments track a list of *dangling out slots*
 * (pointers to State.out / State.out1) that get patched when the fragment is
 * concatenated/closed.
 *
 * The matcher is a true search: it re-seeds the start state at every input
 * position, so it finds matches anywhere (not just anchored at `from`). Each
 * live thread carries the position where it started, so exact match spans are
 * reported. One left-to-right pass, no backtracking blowup. */
#include "search.h"

#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdint.h>
#include <stdio.h>

#define ST_MATCH ((size_t)-1)

typedef struct {
    unsigned char cl[32];   /* 256-bit character class (1 bit per byte) */
    int          is_match;  /* this is the accepting state */
    size_t       out;        /* -1 if none */
    size_t       out1;       /* -1 if none */
} State;

struct Regex {
    State *st;
    size_t n;        /* number of states */
    size_t cap;
    size_t start;    /* start state index */
    int    icase;
};

/* ---- character class helpers ---------------------------------------- */
static void cl_set(unsigned char *cl, int c) {
    if (c < 0 || c > 255) return;
    cl[c >> 3] |= (unsigned char)(1u << (c & 7));
}
static void cl_set_range(unsigned char *cl, int lo, int hi) {
    for (int c = lo; c <= hi; c++) cl_set(cl, c);
}
static int cl_test(const unsigned char *cl, int c) {
    if (c < 0 || c > 255) return 0;
    return (cl[c >> 3] >> (c & 7)) & 1u;
}
static void cl_clear(unsigned char *cl) { memset(cl, 0, 32); }
static void cl_any(unsigned char *cl)   { memset(cl, 0xFF, 32); }

static State *new_state(Regex *r, int is_match) {
    if (r->n == r->cap) {
        r->cap = r->cap ? r->cap * 2 : 16;
        r->st = realloc(r->st, r->cap * sizeof *r->st);
        if (!r->st) abort();
    }
    State *s = &r->st[r->n++];
    memset(s, 0, sizeof *s);
    s->out = s->out1 = ST_MATCH;
    s->is_match = is_match;
    return s;
}

/* ---- fragment: start state + list of dangling out slots to patch ---- */
typedef struct {
    size_t  start;      /* start state index */
    size_t **out;       /* array of pointers to slots (State.out/out1) */
    size_t  n, cap;
} Frag;

static void frag_init(Frag *f) { f->start = 0; f->out = NULL; f->n = f->cap = 0; }
static void frag_free(Frag *f) { free(f->out); f->out = NULL; f->n = f->cap = 0; }
static void frag_add(Frag *f, size_t *slot) {
    if (!slot) return;
    if (f->n == f->cap) {
        f->cap = f->cap ? f->cap * 2 : 8;
        f->out = realloc(f->out, f->cap * sizeof *f->out);
        if (!f->out) abort();
    }
    f->out[f->n++] = slot;
}
static void frag_patch(Frag *f, size_t to) {
    for (size_t i = 0; i < f->n; i++) *f->out[i] = to;
}

typedef struct { const char *p; Regex *r; } Parser;

/* Forward decls. */
static Frag parse_regex(Parser *ps);
static Frag parse_term(Parser *ps);
static Frag parse_factor(Parser *ps);
static Frag parse_atom(Parser *ps);

static Frag parse_atom(Parser *ps) {
    Regex *r = ps->r;
    Frag f; frag_init(&f);
    char c = ps->p[0];

    if (c == '(') {
        ps->p++;
        Frag inner = parse_regex(ps);
        if (ps->p[0] != ')') { frag_free(&inner); return f; }  /* error: empty */
        ps->p++;
        return inner;
    }
    if (c == '[') {
        State *s = new_state(r, 0);
        cl_clear(s->cl);
        ps->p++;
        int neg = 0;
        if (ps->p[0] == '^') { neg = 1; ps->p++; }
        while (ps->p[0] && ps->p[0] != ']') {
            int lo = (unsigned char)ps->p[0]; ps->p++;
            if (ps->p[0] == '-' && ps->p[1] && ps->p[1] != ']') {
                int hi = (unsigned char)ps->p[1]; ps->p += 2;
                if (lo > hi) { int t = lo; lo = hi; hi = t; }
                cl_set_range(s->cl, lo, hi);
            } else {
                cl_set(s->cl, lo);
            }
        }
        if (ps->p[0] == ']') ps->p++;
        if (neg) {
            unsigned char all[32]; cl_any(all);
            for (int i = 0; i < 32; i++) s->cl[i] = (unsigned char)(all[i] & ~s->cl[i]);
        }
        f.start = (size_t)(s - r->st);
        frag_add(&f, &r->st[f.start].out);   /* literal consumes 1 char -> out */
        return f;
    }
    if (c == '.') {
        State *s = new_state(r, 0);
        cl_any(s->cl);
        ps->p++;
        f.start = (size_t)(s - r->st);
        frag_add(&f, &r->st[f.start].out);
        return f;
    }
    /* literal (also handles \\-escapes) */
    {
        int ch = (unsigned char)c;
        if (c == '\\' && ps->p[1]) { ch = (unsigned char)ps->p[1]; ps->p++; }
        ps->p++;
        State *s = new_state(r, 0);
        if (r->icase) { cl_set(s->cl, tolower(ch)); cl_set(s->cl, toupper(ch)); }
        else          { cl_set(s->cl, ch); }
        f.start = (size_t)(s - r->st);
        frag_add(&f, &r->st[f.start].out);
        return f;
    }
}

static Frag parse_factor(Parser *ps) {
    Frag atom = parse_atom(ps);
    char q = ps->p[0];
    if (q == '*' || q == '+' || q == '?') {
        ps->p++;
        Regex *r = ps->r;
        if (q == '*' || q == '+') {
            State *s = new_state(r, 0);     /* split */
            cl_clear(s->cl);
            size_t sidx = (size_t)(s - r->st);
            r->st[sidx].out = atom.start;    /* enter atom */
            frag_patch(&atom, sidx);         /* atom loops back */
            Frag f; frag_init(&f);
            f.start = sidx;
            frag_add(&f, &r->st[sidx].out1);  /* exit dangles */
            frag_free(&atom);
            return f;
        } else { /* '?' : atom optional */
            State *s = new_state(r, 0);      /* split */
            cl_clear(s->cl);
            size_t sidx = (size_t)(s - r->st);
            r->st[sidx].out = atom.start;     /* take atom */
            frag_patch(&atom, sidx);          /* atom exits here */
            Frag f; frag_init(&f);
            f.start = sidx;
            frag_add(&f, &r->st[sidx].out1);  /* skip atom (exit) dangles */
            frag_free(&atom);
            return f;
        }
    }
    return atom;
}

static Frag parse_term(Parser *ps) {
    Frag f; frag_init(&f);
    int first = 1;
    while (ps->p[0] && ps->p[0] != '|' && ps->p[0] != ')') {
        Frag fac = parse_factor(ps);
        if (first) { f = fac; first = 0; }
        else {
            frag_patch(&f, fac.start);   /* prev dangling -> fac.start */
            /* The dangling set is now ONLY fac's unfinished outs (the prev
             * slot was just patched, so it's no longer dangling). Take
             * ownership of fac's out-list; do not free it. */
            free(f.out);
            f.out = fac.out; f.n = fac.n; f.cap = fac.cap;
            fac.out = NULL; fac.n = fac.cap = 0;
            frag_free(&fac);
        }
    }
    return f;
}

static Frag parse_regex(Parser *ps) {
    Frag f = parse_term(ps);
    while (ps->p[0] == '|') {
        ps->p++;
        Frag alt = parse_term(ps);
        Regex *r = ps->r;
        State *s = new_state(r, 0); cl_clear(s->cl);
        size_t sidx = (size_t)(s - r->st);
        r->st[sidx].out  = f.start;
        r->st[sidx].out1 = alt.start;
        Frag merged; frag_init(&merged);
        merged.start = sidx;
        /* dangling: exits of both branches (NOT the split's own out/out1,
         * which are already wired to f.start/alt.start). */
        for (size_t i = 0; i < f.n; i++)   frag_add(&merged, f.out[i]);
        for (size_t i = 0; i < alt.n; i++) frag_add(&merged, alt.out[i]);
        frag_free(&f);
        frag_free(&alt);
        f = merged;
    }
    return f;
}

Regex *regex_compile(const char *pattern, int icase) {
    if (!pattern) return NULL;
    Regex *r = malloc(sizeof *r);
    if (!r) return NULL;
    memset(r, 0, sizeof *r);
    r->icase = icase;

    Parser ps; ps.p = pattern; ps.r = r;
    Frag f = parse_regex(&ps);
    State *m = new_state(r, 1);
    size_t midx = (size_t)(m - r->st);
    frag_patch(&f, midx);
    r->start = f.start;
    frag_free(&f);
    return r;
}

void regex_free(Regex *r) {
    if (!r) return;
    free(r->st);
    free(r);
}

/* ---- NFA simulation (true search) ----------------------------------- */
typedef struct { size_t *st; size_t *sp; size_t n, cap; } List;

static void list_add(List *l, size_t st, size_t start) {
    for (size_t i = 0; i < l->n; i++)
        if (l->st[i] == st && l->sp[i] == start) return;   /* dedupe */
    if (l->n == l->cap) {
        l->cap = l->cap ? l->cap * 2 : 16;
        l->st = realloc(l->st, l->cap * sizeof *l->st);
        l->sp = realloc(l->sp, l->cap * sizeof *l->sp);
        if (!l->st || !l->sp) abort();
    }
    l->st[l->n] = st; l->sp[l->n] = start; l->n++;
}

/* Add state `s` and follow epsilon edges (recursively) into the list. */
static void add_state(Regex *r, List *l, size_t s, size_t start) {
    if (s == ST_MATCH) return;
    State *st = &r->st[s];
    if (st->is_match) { list_add(l, s, start); return; }   /* accepting state */
    int is_eps = 1;
    for (int i = 0; i < 32; i++) if (st->cl[i]) { is_eps = 0; break; }
    if (is_eps) {
        if (st->out  != ST_MATCH) add_state(r, l, st->out,  start);
        if (st->out1 != ST_MATCH) add_state(r, l, st->out1, start);
        return;
    }
    list_add(l, s, start);
}

int regex_find_from(const Regex *re_, const char *text, size_t len,
                     size_t from, size_t *m_start, size_t *m_end) {
    Regex *r = (Regex *)re_;
    if (!r || from > len) return 0;
    List cur, nxt;
    cur.st = cur.sp = NULL; cur.n = cur.cap = 0;
    nxt.st = nxt.sp = NULL; nxt.n = nxt.cap = 0;

    for (size_t i = from; i <= len; i++) {
        add_state(r, &cur, r->start, i);          /* a new match may start at i */
        /* a completed match may already be live in cur */
        int matched = 0; size_t mstart = i;
        for (size_t k = 0; k < cur.n; k++) {
            if (r->st[cur.st[k]].is_match) { matched = 1; mstart = cur.sp[k]; break; }
        }
        if (matched) {
            if (m_start) *m_start = mstart;
            if (m_end)   *m_end   = i;
            free(cur.st); free(cur.sp); free(nxt.st); free(nxt.sp);
            return 1;
        }
        if (i == len) break;
        int c = (unsigned char)text[i];
        nxt.n = 0;
        for (size_t k = 0; k < cur.n; k++) {
            State *st = &r->st[cur.st[k]];
            if (cl_test(st->cl, c)) {
                add_state(r, &nxt, st->out, cur.sp[k]);
            }
        }
        List t = cur; cur = nxt; nxt = t;
    }
    free(cur.st); free(cur.sp); free(nxt.st); free(nxt.sp);
    return 0;
}

int regex_find(const Regex *re, const char *text, size_t len,
                size_t *m_start, size_t *m_end) {
    return regex_find_from(re, text, len, 0, m_start, m_end);
}

size_t search_literal(const char *hay, size_t hlen,
                      const char *needle, size_t nlen, size_t from) {
    if (nlen == 0) return from <= hlen ? from : SIZE_MAX;
    if (nlen > hlen) return SIZE_MAX;
    if (from > hlen - nlen) return SIZE_MAX;
    for (size_t i = from; i <= hlen - nlen; i++) {
        size_t j = 0;
        for (; j < nlen; j++) if (hay[i + j] != needle[j]) break;
        if (j == nlen) return i;
    }
    return SIZE_MAX;
}
