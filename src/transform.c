/* transform.c -- text transforms: sort selected lines and case conversion.
 * Headless, reusable by the agent protocol and the interactive UI. Each op
 * records one undo via doc_replace. Clean C11. */
#include "transform.h"
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <strings.h>   /* strcasecmp */

/* Return the [start,end) byte range covering whole lines of the selection
 * (or the whole doc if no selection). Caller frees *txt via free(). */
static void selected_full_lines(const Doc *d, size_t *start, size_t *end,
                                char **txt){
    size_t s, e;
    if (doc_has_selection(d)){
        s = doc_sel_start(d);
        e = doc_sel_end(d);
        if (s > e){ size_t t = s; s = e; e = t; }
    } else {
        s = 0; e = doc_length(d);
    }
    char *t = doc_text(d);
    if (!t){ *start = s; *end = e; *txt = NULL; return; }
    size_t len = doc_length(d);
    /* snap s back to the start of its line */
    while (s > 0 && t[s-1] != '\n') s--;
    /* snap e forward to just past its line's newline */
    while (e < len && t[e] != '\n') e++;
    if (e < len) e++;               /* include the newline */
    *start = s; *end = e; *txt = t;
}

/* ---- case conversion --------------------------------------------------- */
int transform_case(Doc *d, int which){
    size_t s, e;
    char *txt;
    selected_full_lines(d, &s, &e, &txt);
    if (!txt) return -1;
    size_t n = e - s;
    char *out = malloc(n + 1);
    if (!out){ free(txt); return -1; }
    int prev_word = 0;
    for (size_t i = 0; i < n; i++){
        unsigned char c = (unsigned char)txt[s + i];
        int is_word = isalnum(c) || c == '_';
        if (which == TRANSFORM_UPPER)      out[i] = (char)toupper(c);
        else if (which == TRANSFORM_LOWER) out[i] = (char)tolower(c);
        else { /* title */
            if (is_word && !prev_word) out[i] = (char)toupper(c);
            else                       out[i] = (char)tolower(c);
        }
        prev_word = is_word;
    }
    out[n] = '\0';
    int changed = (memcmp(out, txt + s, n) != 0);
    if (changed) doc_replace(d, s, e, out);
    free(out);
    free(txt);
    return changed;
}

/* ---- line sort --------------------------------------------------------- */
static int cmp_asc(const void *a, const void *b){ return strcmp(*(const char**)a, *(const char**)b); }
static int cmp_desc(const void *a, const void *b){ return strcmp(*(const char**)b, *(const char**)a); }
static int cmp_asc_ic(const void *a, const void *b){ return strcasecmp(*(const char**)a, *(const char**)b); }
static int cmp_desc_ic(const void *a, const void *b){ return strcasecmp(*(const char**)b, *(const char**)a); }

int transform_sort_lines(Doc *d, int which){
    size_t s, e;
    char *txt;
    selected_full_lines(d, &s, &e, &txt);
    if (!txt) return -1;
    size_t n = e - s;
    char *region = malloc(n + 1);
    if (!region){ free(txt); return -1; }
    memcpy(region, txt + s, n);
    region[n] = '\0';
    free(txt);

    int had_trailing = (n > 0 && region[n-1] == '\n');

    /* split into lines */
    char **lines = NULL;
    size_t nlines = 0;
    char *p = region;
    while (*p){
        char *nl = strchr(p, '\n');
        char *line = p;
        if (nl) *nl = '\0';
        char **np = realloc(lines, (nlines + 1) * sizeof *np);
        if (!np){ free(region); free(lines); return -1; }
        lines = np;
        lines[nlines++] = line;
        if (!nl) break;
        p = nl + 1;
    }

    int (*cmp)(const void*, const void*) =
        (which == SORT_ASC)   ? cmp_asc :
        (which == SORT_DESC)  ? cmp_desc :
        (which == SORT_ASC_IC)? cmp_asc_ic : cmp_desc_ic;

    /* original line order, joined, for no-op detection */
    char *orig = malloc(n + 1);
    if (!orig){ free(region); free(lines); return -1; }
    size_t olen = 0;
    for (size_t i = 0; i < nlines; i++){
        size_t llen = strlen(lines[i]);
        memcpy(orig + olen, lines[i], llen); olen += llen;
        if (i + 1 < nlines || had_trailing) orig[olen++] = '\n';
    }
    orig[olen] = '\0';

    qsort(lines, nlines, sizeof *lines, cmp);

    size_t cap = n + 2, len = 0;
    char *out = malloc(cap);
    if (!out){ free(region); free(lines); free(orig); return -1; }
    for (size_t i = 0; i < nlines; i++){
        size_t llen = strlen(lines[i]);
        if (len + llen + 2 > cap){ cap = len + llen + 2; out = realloc(out, cap); }
        memcpy(out + len, lines[i], llen); len += llen;
        if (i + 1 < nlines || had_trailing) out[len++] = '\n';
    }
    out[len] = '\0';
    free(region);
    free(lines);

    int changed = 0;
    if (memcmp(out, orig, olen) != 0){
        doc_replace(d, s, e, out);
        changed = 1;
    }
    free(out);
    free(orig);
    return changed;
}
