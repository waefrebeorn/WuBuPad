/* ui_find.c -- find/replace controller (see ui_find.h). */
#include "ui_find.h"
#include "search.h"
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#define MAX_MATCHES 100000

struct UIFind {
    char  *pattern;
    int    regex, icase, bad;
    size_t *ms, *me;     /* match start/end byte ranges */
    long   n;            /* match count */
    long   cur;          /* active index */
};

UIFind *ui_find_create(void) {
    UIFind *f = calloc(1, sizeof *f);
    return f;
}

void ui_find_free(UIFind *f) {
    if (!f) return;
    free(f->pattern);
    free(f->ms); free(f->me);
    free(f);
}

const char *ui_find_pattern(const UIFind *f) { return f ? f->pattern : ""; }
int ui_find_is_regex(const UIFind *f) { return f ? f->regex : 0; }
int ui_find_is_icase(const UIFind *f) { return f ? f->icase : 0; }
int ui_find_bad_pattern(const UIFind *f) { return f ? f->bad : 0; }
long ui_find_count(const UIFind *f) { return f ? f->n : 0; }

static void store_pattern(UIFind *f, const char *p) {
    free(f->pattern);
    f->pattern = p && *p ? strdup(p) : strdup("");
}

long ui_find_run(UIFind *f, const char *text, size_t len,
                 const char *pattern, int regex, int icase) {
    if (!f) return -1;
    store_pattern(f, pattern);
    f->regex = regex; f->icase = icase; f->bad = 0; f->n = 0; f->cur = 0;
    free(f->ms); free(f->me); f->ms = f->me = NULL;
    if (!pattern || !*pattern) return 0;

    if (regex) {
        /* cheap balance check so a malformed pattern is reported as bad
         * (the engine itself is lenient about unclosed groups). */
        int depth = 0;
        for (const char *p = pattern; *p; p++) {
            if (*p == '(') depth++;
            else if (*p == ')') { depth--; if (depth < 0) break; }
        }
        if (depth != 0) { f->bad = 1; return -1; }
        Regex *re = regex_compile(pattern, icase);
        if (!re) { f->bad = 1; return -1; }
        size_t from = 0, s, e;
        while (regex_find_from(re, text, len, from, &s, &e)) {
            if (f->n >= MAX_MATCHES) break;
            f->ms = realloc(f->ms, (size_t)(f->n + 1) * sizeof *f->ms);
            f->me = realloc(f->me, (size_t)(f->n + 1) * sizeof *f->me);
            f->ms[f->n] = s; f->me[f->n] = e;
            f->n++;
            from = e > from ? e : from + 1;   /* advance past this match */
            if (from >= len) break;
        }
        regex_free(re);
    } else {
        size_t from = 0;
        for (;;) {
            size_t s;
            if (icase) {
                /* case-insensitive literal scan */
                s = SIZE_MAX;
                size_t plen = strlen(pattern);
                if (plen == 0) break;
                for (size_t i = from; i + plen <= len; i++) {
                    size_t k = 0;
                    for (; k < plen; k++) {
                        char a = text[i + k], b = pattern[k];
                        if (a >= 'A' && a <= 'Z') a = (char)(a - 'A' + 'a');
                        if (b >= 'A' && b <= 'Z') b = (char)(b - 'A' + 'a');
                        if (a != b) break;
                    }
                    if (k == plen) { s = i; break; }
                }
            } else {
                s = search_literal(text, len, pattern, strlen(pattern), from);
            }
            if (s == SIZE_MAX) break;
            if (f->n >= MAX_MATCHES) break;
            f->ms = realloc(f->ms, (size_t)(f->n + 1) * sizeof *f->ms);
            f->me = realloc(f->me, (size_t)(f->n + 1) * sizeof *f->me);
            f->ms[f->n] = s; f->me[f->n] = s + strlen(pattern);
            f->n++;
            from = f->me[f->n - 1];
        }
    }
    return f->n;
}

int ui_find_active(const UIFind *f, size_t *start, size_t *end) {
    if (!f || f->n == 0) return 0;
    if (start) *start = f->ms[f->cur];
    if (end)   *end   = f->me[f->cur];
    return 1;
}

int ui_find_next(UIFind *f) {
    if (!f || f->n == 0) return 0;
    f->cur = (f->cur + 1) % f->n;
    return 1;
}

int ui_find_prev(UIFind *f) {
    if (!f || f->n == 0) return 0;
    f->cur = (f->cur - 1 + f->n) % f->n;
    return 1;
}

int ui_find_replace(const UIFind *f, const char *text, size_t len,
                    const char *repl, char **out, size_t *newlen) {
    if (!f || f->n == 0) return -1;
    size_t s = f->ms[f->cur], e = f->me[f->cur];
    size_t rlen = repl ? strlen(repl) : 0;
    size_t rest = len - e;
    char *buf = malloc(len - (e - s) + rlen + 1);
    if (!buf) return -1;
    memcpy(buf, text, s);
    if (rlen) memcpy(buf + s, repl, rlen);
    memcpy(buf + s + rlen, text + e, rest);
    buf[len - (e - s) + rlen] = 0;
    *out = buf;
    *newlen = len - (e - s) + rlen;
    return 0;
}

long ui_find_replace_all(const UIFind *f, const char *text, size_t len,
                         const char *repl, char **out, size_t *newlen) {
    if (!f || f->n == 0) { *out = strdup(text ? text : ""); *newlen = len; return 0; }
    size_t rlen = repl ? strlen(repl) : 0;
    /* worst case size; we compact as we go */
    char *buf = malloc(len + (size_t)f->n * (rlen + 1) + 1);
    if (!buf) return -1;
    size_t pos = 0, w = 0;
    for (long i = 0; i < f->n; i++) {
        size_t s = f->ms[i], e = f->me[i];
        memcpy(buf + w, text + pos, s - pos);
        w += s - pos;
        if (rlen) { memcpy(buf + w, repl, rlen); w += rlen; }
        pos = e;
    }
    memcpy(buf + w, text + pos, len - pos);
    w += len - pos;
    buf[w] = 0;
    *out = buf;
    *newlen = w;
    return f->n;
}
