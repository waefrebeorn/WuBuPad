/* mdpreview.c -- Markdown -> HTML. See mdpreview.h. */
#include "mdpreview.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>

/* append with HTML-escaping of special chars */
static void esc_append(char **p, size_t *len, size_t *cap, const char *s, size_t n, int escape) {
    for (size_t i = 0; i < n; i++) {
        char c = s[i];
        const char *rep = NULL;
        if (escape) {
            if (c == '&') rep = "&amp;";
            else if (c == '<') rep = "&lt;";
            else if (c == '>') rep = "&gt;";
        }
        size_t need = rep ? strlen(rep) : 1;
        if (*len + need + 1 > *cap) {
            size_t ncap = *cap ? *cap * 2 : 256;
            while (*len + need + 1 > ncap) ncap *= 2;
            char *np = realloc(*p, ncap);
            if (!np) return;
            *p = np; *cap = ncap;
        }
        if (rep) { strcpy(*p + *len, rep); *len += strlen(rep); }
        else { (*p)[(*len)++] = c; }
    }
    (*p)[*len] = 0;
}

/* inline formatting: **bold**, *italic*, `code` */
static void emit_inline(char **p, size_t *l, size_t *c, const char *s, size_t n) {
    size_t i = 0;
    while (i < n) {
        if (i + 1 < n && s[i] == '*' && s[i+1] == '*') {
            size_t j = i + 2; while (j + 1 < n && !(s[j]=='*'&&s[j+1]=='*')) j++;
            esc_append(p, l, c, "<strong>", 8, 0);
            emit_inline(p, l, c, s + i + 2, (j > i+2 ? j - (i+2) : 0));
            esc_append(p, l, c, "</strong>", 9, 0);
            i = (j + 1 < n && s[j]=='\0') ? n : (j < n ? j + 2 : n);
        } else if (s[i] == '*') {
            size_t j = i + 1; while (j < n && s[j] != '*') j++;
            esc_append(p, l, c, "<em>", 4, 0);
            emit_inline(p, l, c, s + i + 1, (j > i+1 ? j - (i+1) : 0));
            esc_append(p, l, c, "</em>", 5, 0);
            i = (j < n) ? j + 1 : n;
        } else if (s[i] == '`') {
            size_t j = i + 1; while (j < n && s[j] != '`') j++;
            esc_append(p, l, c, "<code>", 6, 0);
            esc_append(p, l, c, s + i + 1, (j > i+1 ? j - (i+1) : 0), 1);
            esc_append(p, l, c, "</code>", 7, 0);
            i = (j < n) ? j + 1 : n;
        } else {
            esc_append(p, l, c, s + i, 1, 1); i++;
        }
    }
}

char *mdpreview_render(const char *md, const char *title) {
    size_t cap = 1024, len = 0;
    char *out = malloc(cap);
    if (!out) return NULL;
    const char *t = title ? title : "preview";
    len += (size_t)snprintf(out, cap, "<!doctype html><html><head><meta charset=\"utf-8\"><title>%s</title></head><body>\n", t);

    size_t i = 0, L = md ? strlen(md) : 0;
    int in_code = 0;
    while (i < L) {
        /* line */
        size_t e = i; while (e < L && md[e] != '\n') e++;
        size_t llen = e - i;
        const char *line = md + i;
        /* fenced code */
        if (llen >= 3 && strncmp(line, "```", 3) == 0) {
            if (!in_code) { esc_append(&out,&len,&cap,"<pre><code>",11,0); in_code = 1; }
            else { esc_append(&out,&len,&cap,"</code></pre>\n",13,0); in_code = 0; }
            i = (e < L) ? e + 1 : L; continue;
        }
        if (in_code) { esc_append(&out,&len,&cap,line,llen,1); esc_append(&out,&len,&cap,"\n",1,0); i=(e<L)?e+1:L; continue; }
        /* heading */
        int h = 0; while ((size_t)h < llen && h < 6 && line[h] == '#') h++;
        if (h > 0 && ((size_t)h == llen || line[h] == ' ')) {
            size_t txt0 = ((size_t)h < llen) ? i + h + 1 : i + h;
            char tag[8]; snprintf(tag, sizeof tag, "h%d", h);
            esc_append(&out,&len,&cap,"<",1,0); esc_append(&out,&len,&cap,tag,strlen(tag),0); esc_append(&out,&len,&cap,">",1,0);
            emit_inline(&out,&len,&cap, md+txt0, (e>txt0?e-txt0:0));
            esc_append(&out,&len,&cap,"</",2,0); esc_append(&out,&len,&cap,tag,strlen(tag),0); esc_append(&out,&len,&cap,">\n",2,0);
            i=(e<L)?e+1:L; continue;
        }
        /* hr */
        if (llen >= 3 && (strncmp(line,"---",3)==0 || strncmp(line,"***",3)==0)) {
            esc_append(&out,&len,&cap,"<hr>\n",5,0); i=(e<L)?e+1:L; continue;
        }
        /* blockquote */
        if (llen > 0 && line[0] == '>') {
            const char *b = line + 1;
            if (*b == ' ') b++;
            esc_append(&out,&len,&cap,"<blockquote>",12,0);
            emit_inline(&out,&len,&cap, b, (size_t)(line + llen - b));
            esc_append(&out,&len,&cap,"</blockquote>\n",13,0); i=(e<L)?e+1:L; continue;
        }
        /* unordered list */
        if (llen >= 2 && (line[0]=='-'||line[0]=='*') && line[1]==' ') {
            esc_append(&out,&len,&cap,"<li>",4,0);
            emit_inline(&out,&len,&cap, line+2, llen-2);
            esc_append(&out,&len,&cap,"</li>\n",6,0); i=(e<L)?e+1:L; continue;
        }
        /* ordered list */
        if (llen >= 3 && isdigit((unsigned char)line[0]) && line[1]=='.' && line[2]==' ') {
            esc_append(&out,&len,&cap,"<li>",4,0);
            emit_inline(&out,&len,&cap, line+3, llen-3);
            esc_append(&out,&len,&cap,"</li>\n",6,0); i=(e<L)?e+1:L; continue;
        }
        /* blank line */
        if (llen == 0) { i=(e<L)?e+1:L; continue; }
        /* paragraph */
        esc_append(&out,&len,&cap,"<p>",3,0);
        emit_inline(&out,&len,&cap, line, llen);
        esc_append(&out,&len,&cap,"</p>\n",5,0);
        i=(e<L)?e+1:L;
    }
    if (in_code) esc_append(&out,&len,&cap,"</code></pre>\n",13,0);
    esc_append(&out,&len,&cap,"</body></html>\n",15,0);
    return out;
}

size_t mdpreview_render_buf(const char *md, const char *title, char *out, size_t cap) {
    char *full = mdpreview_render(md, title);
    if (!full) return (size_t)-1;
    size_t n = strlen(full);
    if (n + 1 > cap) { free(full); return (size_t)-1; }
    memcpy(out, full, n + 1);
    free(full);
    return n;
}
