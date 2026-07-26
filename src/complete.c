/* complete.c -- word completion + symbol extraction (see complete.h). */
#include "complete.h"
#include "lex.h"

#include <stdlib.h>
#include <string.h>
#include <ctype.h>

/* gather identifiers from text using the lexer (lang-agnostic identifiers) */
static size_t collect_idents(const char *text, size_t len,
                             char ***out) {
    Lex *lx = lex_create("c");
    char **ids = NULL;
    size_t n = 0, cap = 0;
    size_t off = 0;
    while (off < len) {
        LexSpan spans[16];
        size_t ns = lex_run(lx, text + off, len - off, spans, 16);
        if (ns == 0) break;
        for (size_t s = 0; s < ns; s++) {
            if (spans[s].kind != TK_IDENT &&
                spans[s].kind != TK_KEYWORD && spans[s].kind != TK_TYPE) continue;
            size_t l = spans[s].end - spans[s].start;
            if (l == 0) continue;
            /* dedupe against what we already have */
            int dup = 0;
            for (size_t i = 0; i < n; i++)
                if (strlen(ids[i]) == l && memcmp(ids[i], text + off + spans[s].start, l) == 0) { dup = 1; break; }
            if (dup) continue;
            if (n + 1 >= cap) { cap = cap ? cap * 2 : 64; ids = realloc(ids, cap * sizeof(char*)); }
            char *w = malloc(l + 1);
            memcpy(w, text + off + spans[s].start, l);
            w[l] = 0;
            ids[n++] = w;
        }
        /* advance past the last span consumed */
        size_t last = spans[ns - 1].end;
        if (last == 0) last = 1;
        off += last;
    }
    lex_free(lx);
    *out = ids;
    return n;
}

char **doc_complete(const char *text, size_t len,
                    const char *prefix, size_t *n) {
    char **ids = NULL;
    size_t m = collect_idents(text, len, &ids);
    size_t plen = prefix ? strlen(prefix) : 0;
    char **out = NULL;
    size_t on = 0, ocap = 0;
    for (size_t i = 0; i < m; i++) {
        size_t il = strlen(ids[i]);
        if (plen && (il < plen || memcmp(ids[i], prefix, plen) != 0)) continue;
        if (plen && il == plen) continue; /* skip exact prefix match */
        if (on + 1 >= ocap) { ocap = ocap ? ocap * 2 : 16; out = realloc(out, ocap * sizeof(char*)); }
        out[on++] = ids[i];   /* transfer ownership */
        ids[i] = NULL;
    }
    for (size_t i = 0; i < m; i++) free(ids[i]);
    free(ids);
    *n = on;
    return out;
}

void doc_complete_free(char **list, size_t n) {
    for (size_t i = 0; i < n; i++) free(list[i]);
    free(list);
}

DocSymbol *doc_symbols(const char *text, size_t len, size_t *n) {
    Lex *lx = lex_create("c");
    DocSymbol *sy = NULL;
    size_t cnt = 0, cap = 0;
    size_t off = 0, line = 0;
    while (off < len) {
        LexSpan spans[16];
        size_t ns = lex_run(lx, text + off, len - off, spans, 16);
        if (ns == 0) break;
        for (size_t s = 0; s < ns; s++) {
            if (spans[s].kind != TK_IDENT) {
                /* track newlines manually for line numbers */
                for (size_t k = spans[s].start; k < spans[s].end; k++)
                    if (text[off + k] == '\n') line++;
                continue;
            }
            size_t l = spans[s].end - spans[s].start;
            int is_func = 0;
            /* peek next non-space char for '(' */
            size_t p = spans[s].end;
            while (p < len && (text[off + p] == ' ' || text[off + p] == '\t')) p++;
            if (p < len && text[off + p] == '(') is_func = 1;
            if (cnt + 1 >= cap) { cap = cap ? cap * 2 : 64; sy = realloc(sy, cap * sizeof(DocSymbol)); }
            sy[cnt].name = malloc(l + 1);
            memcpy(sy[cnt].name, text + off + spans[s].start, l);
            sy[cnt].name[l] = 0;
            sy[cnt].line = line;
            sy[cnt].is_func = is_func;
            cnt++;
            for (size_t k = spans[s].start; k < spans[s].end; k++)
                if (text[off + k] == '\n') line++;
        }
        size_t last = spans[ns - 1].end;
        if (last == 0) last = 1;
        off += last;
    }
    lex_free(lx);
    *n = cnt;
    return sy;
}

void doc_symbols_free(DocSymbol *syms, size_t n) {
    if (!syms) return;
    for (size_t i = 0; i < n; i++) free(syms[i].name);
    free(syms);
}
