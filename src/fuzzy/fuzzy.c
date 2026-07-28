/* fuzzy.c -- Atom-style fuzzy scorer. See fuzzy.h. */
#include "fuzzy.h"
#include <string.h>
#include <ctype.h>

static int ilower(int c){ return tolower((unsigned char)c); }

static int is_sep(int c){ return c=='/'||c=='.'||c=='_'||c=='-'||c==' '||c=='\\'
                             || c==':'; }

long fuzzy_score(const char *query, const char *cand) {
    if (!query || !cand) return -1;
    size_t ql = strlen(query);
    if (ql == 0) return 0;           /* empty query matches everything */
    size_t cl = strlen(cand);
    if (ql > cl) return -1;

    long score = 0;
    size_t qi = 0, ci = 0;
    int streak = 0;
    int prev_match = -1;
    while (qi < ql && ci < cl) {
        if (ilower((unsigned char)query[qi]) == ilower((unsigned char)cand[ci])) {
            long bonus = 1;
            if (ci == 0) bonus += 8;            /* start-of-string */
            else if (is_sep(cand[ci-1])) bonus += 6; /* after separator */
            if (prev_match >= 0 && ci == (size_t)prev_match + 1) {
                streak++;
                bonus += 3 + streak * 2;        /* contiguity ramp */
            } else {
                streak = 0;
            }
            score += bonus;
            prev_match = (int)ci;
            qi++;
        }
        ci++;
    }
    if (qi < ql) return -1;             /* not a subsequence */
    /* reward full coverage lightly */
    if (ql == cl) score += 10;
    return score;
}

int fuzzy_match(const char *query, const char *cand) {
    return fuzzy_score(query, cand) >= 0;
}

size_t fuzzy_top(const char **items, size_t n, const char *query,
                 size_t *out_idx, long *out_score, size_t cap) {
    if (!items || !out_idx || !out_score || cap == 0) return 0;
    /* simple O(n*cap) selection; fine for palette/file lists */
    size_t written = 0;
    for (size_t i = 0; i < n && written < cap; i++) {
        long s = fuzzy_score(query, items[i]);
        if (s < 0) continue;
        /* insert into the small result list (sorted desc by score) */
        size_t pos = written;
        while (pos > 0 && out_score[pos-1] < s) { pos--; }
        for (size_t k = written; k > pos; k--) {
            out_idx[k]   = out_idx[k-1];
            out_score[k] = out_score[k-1];
        }
        out_idx[pos]   = i;
        out_score[pos] = s;
        written++;
    }
    return written;
}
