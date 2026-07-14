/* diff.c -- line diff via longest-common-subsequence (Wagner-Fischer style).
 * Self-contained C11. Produces an edit script (equal / delete / insert) by
 * computing the edit distance over lines and backtracing the optimal path.
 * O(n*m) time/space on the number of lines; ample for editor compare. */
#include "diff.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

struct Diff {
    DiffEdit *e;
    size_t    n, cap;
};

static void push(Diff *d, DiffOp op, size_t ai, size_t bi) {
    if (d->n == d->cap) {
        d->cap = d->cap ? d->cap * 2 : 64;
        d->e = realloc(d->e, d->cap * sizeof *d->e);
        if (!d->e) abort();
    }
    d->e[d->n].op = op; d->e[d->n].a_idx = ai; d->e[d->n].b_idx = bi;
    d->n++;
}

static int eq(const char *const *a, size_t i, const char *const *b, size_t j) {
    return strcmp(a[i], b[j]) == 0;
}

Diff *diff_lines(const char *const *a, size_t na, const char *const *b, size_t nb) {
    Diff *d = malloc(sizeof *d);
    if (!d) return NULL;
    memset(d, 0, sizeof *d);
    if (na == 0 && nb == 0) return d;

    /* dp[i][j] = edit distance between a[i..] and b[j..]; size (na+1)*(nb+1) */
    size_t w = nb + 1;
    size_t *dp = malloc((na + 1) * w * sizeof *dp);
    if (!dp) { free(d); return NULL; }

    /* base: deleting all of A, or inserting all of B */
    for (size_t i = 0; i <= na; i++) dp[i * w + nb] = na - i;
    for (size_t j = 0; j <= nb; j++) dp[na * w + j] = nb - j;

    for (size_t i = na; i-- > 0; ) {        /* i from na-1 down to 0 */
        for (size_t j = nb; j-- > 0; ) {    /* j from nb-1 down to 0 */
            if (eq(a, i, b, j))
                dp[i * w + j] = dp[(i + 1) * w + (j + 1)];
            else {
                size_t del = dp[(i + 1) * w + j] + 1;     /* drop a[i] */
                size_t ins = dp[i * w + (j + 1)] + 1;      /* add b[j] */
                dp[i * w + j] = (del < ins) ? del : ins;
            }
        }
    }

    /* backtrace from (0,0) building the edit script */
    size_t i = 0, j = 0;
    while (i < na && j < nb) {
        if (eq(a, i, b, j)) {
            push(d, DIFF_EQ, i, j); i++; j++;
        } else {
            size_t del = dp[(i + 1) * w + j];
            size_t ins = dp[i * w + (j + 1)];
            if (del <= ins) { push(d, DIFF_DEL, i, j); i++; }
            else            { push(d, DIFF_INS, i, j); j++; }
        }
    }
    while (i < na) { push(d, DIFF_DEL, i, j); i++; }
    while (j < nb) { push(d, DIFF_INS, i, j); j++; }

    free(dp);
    return d;
}

size_t diff_count(const Diff *d) { return d ? d->n : 0; }
const DiffEdit *diff_get(const Diff *d, size_t i) {
    if (!d || i >= d->n) return NULL;
    return &d->e[i];
}

void diff_free(Diff *d) {
    if (!d) return;
    free(d->e);
    free(d);
}

char *diff_unified(const Diff *d, const char *const *a, const char *const *b,
                   const char *name_a, const char *name_b) {
    if (!d) return NULL;
    size_t cap = 1024, n = 0;
    char *out = malloc(cap);
    if (!out) return NULL;
    #define APP(...) do { \
        int _w = snprintf(out + n, cap - n, __VA_ARGS__); \
        if (_w < 0) _w = 0; \
        if ((size_t)_w >= cap - n) { cap = n + (size_t)_w + 1024; out = realloc(out, cap); _w = snprintf(out + n, cap - n, __VA_ARGS__); } \
        n += (size_t)_w; \
    } while (0)

    APP("--- %s\n", name_a ? name_a : "a");
    APP("+++ %s\n", name_b ? name_b : "b");
    for (size_t i = 0; i < d->n; i++) {
        const DiffEdit *e = &d->e[i];
        if (e->op == DIFF_EQ)      APP(" %s\n", a[e->a_idx]);
        else if (e->op == DIFF_DEL) APP("-%s\n", a[e->a_idx]);
        else                        APP("+%s\n", b[e->b_idx]);
    }
    #undef APP
    return out;
}
