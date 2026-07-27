/* lex.c -- language registry + C and JSON lexers. Self-contained C11. */
#include "lex.h"

#include <stdlib.h>
#include <string.h>
#include <ctype.h>

struct Lex {
    char   lang[16];
    size_t (*fn)(const char *, size_t, LexSpan *, size_t);
};

/* ---- small token emitter --------------------------------------------- */
typedef struct { LexSpan *out; size_t cap; size_t n; } Emit;

static void emit(Emit *e, size_t start, size_t end, LexTok k) {
    if (e->n >= e->cap) return;
    e->out[e->n].start = start;
    e->out[e->n].end   = end;
    e->out[e->n].kind  = k;
    e->n++;
}

static int kw_match(const char *p, size_t len, const char *const *kws, size_t n) {
    for (size_t i = 0; i < n; i++)
        if (strlen(kws[i]) == len && strncmp(p, kws[i], len) == 0) return 1;
    return 0;
}

/* ---- C lexer --------------------------------------------------------- */
static const char *c_keywords[] = {
    "if","else","for","while","do","switch","case","default","break","continue",
    "return","goto","sizeof","typeof","asm"
};
static const char *c_types[] = {
    "int","char","float","double","void","long","short","signed","unsigned",
    "const","volatile","static","extern","register","auto","inline","struct",
    "union","enum","typedef","_Bool","_Complex","_Atomic","restrict","_Alignof",
    "size_t","wchar_t","bool"
};
/* crude preprocessor directive set */
static const char *c_pp[] = {
    "include","define","ifdef","ifndef","endif","if","else","elif","undef",
    "pragma","error","warning","line"
};
static size_t lex_c(const char *t, size_t len, LexSpan *out, size_t cap) {
    (void)c_pp;
    Emit e = { out, cap, 0 };
    size_t i = 0;
    while (i < len) {
        char c = t[i];
        size_t start = i;

        /* whitespace */
        if (c == ' ' || c == '\t' || c == '\n' || c == '\r') {
            while (i < len && (t[i]==' '||t[i]=='\t'||t[i]=='\n'||t[i]=='\r')) i++;
            emit(&e, start, i, TK_WHITESPACE); continue;
        }
        /* line comment */
        if (c == '/' && i + 1 < len && t[i+1] == '/') {
            i += 2;
            while (i < len && t[i] != '\n') i++;
            emit(&e, start, i, TK_COMMENT); continue;
        }
        /* block comment */
        if (c == '/' && i + 1 < len && t[i+1] == '*') {
            i += 2;
            while (i + 1 < len && !(t[i]=='*' && t[i+1]=='/')) i++;
            if (i + 1 < len) i += 2;
            emit(&e, start, i, TK_COMMENT); continue;
        }
        /* preprocessor line: '#' at column 0 of a line */
        if (c == '#') {
            /* find end of directive word after '#' */
            size_t j = i + 1;
            while (j < len && (t[j]==' '||t[j]=='\t')) j++;
            size_t w0 = j;
            while (j < len && (isalnum((unsigned char)t[j]) || t[j]=='_')) j++;
            emit(&e, start, i, TK_PREPROC);
            if (j > w0) emit(&e, w0, j, TK_PREPROC);
            i = j;
            /* rest of the line is preproc-ish; mark as preproc to end */
            size_t line_end = j;
            while (line_end < len && t[line_end] != '\n') line_end++;
            emit(&e, j, line_end, TK_PREPROC);
            i = line_end; continue;
        }
        /* string */
        if (c == '"') {
            i++;
            while (i < len && t[i] != '"') {
                if (t[i] == '\\' && i + 1 < len) i++;
                i++;
            }
            if (i < len) i++; /* closing quote */
            emit(&e, start, i, TK_STRING); continue;
        }
        /* char literal */
        if (c == '\'') {
            i++;
            while (i < len && t[i] != '\'') {
                if (t[i] == '\\' && i + 1 < len) i++;
                i++;
            }
            if (i < len) i++;
            emit(&e, start, i, TK_CHAR); continue;
        }
        /* number */
        if (isdigit((unsigned char)c) || (c=='.' && i+1<len && isdigit((unsigned char)t[i+1]))) {
            while (i < len && (isalnum((unsigned char)t[i]) || t[i]=='.' || t[i]=='_')) i++;
            emit(&e, start, i, TK_NUMBER); continue;
        }
        /* identifier / keyword */
        if (isalpha((unsigned char)c) || c == '_') {
            while (i < len && (isalnum((unsigned char)t[i]) || t[i]=='_')) i++;
            size_t wlen = i - start;
            if (kw_match(t + start, wlen, c_keywords, sizeof c_keywords / sizeof *c_keywords))
                emit(&e, start, i, TK_KEYWORD);
            else if (kw_match(t + start, wlen, c_types, sizeof c_types / sizeof *c_types))
                emit(&e, start, i, TK_TYPE);
            else
                emit(&e, start, i, TK_IDENT);
            continue;
        }
        /* operators / punctuation */
        if (strchr("+-*/%=&|!<>^~?:", c)) {
            while (i < len && strchr("+-*/%=&|!<>^~?:", t[i])) i++;
            emit(&e, start, i, TK_OPERATOR); continue;
        }
        /* other punctuation */
        if (strchr("(){}[];,.", c)) {
            i++;
            emit(&e, start, i, TK_PUNCT); continue;
        }
        /* fallback */
        i++;
        emit(&e, start, i, TK_TEXT);
    }
    return e.n;
}

/* ---- JSON lexer ------------------------------------------------------ */
static size_t lex_json(const char *t, size_t len, LexSpan *out, size_t cap) {
    Emit e = { out, cap, 0 };
    size_t i = 0;
    while (i < len) {
        char c = t[i];
        size_t start = i;
        if (c==' '||c=='\t'||c=='\n'||c=='\r') {
            while (i<len && (t[i]==' '||t[i]=='\t'||t[i]=='\n'||t[i]=='\r')) i++;
            emit(&e, start, i, TK_WHITESPACE); continue;
        }
        if (c=='"') {
            i++;
            while (i<len && t[i]!='"') { if (t[i]=='\\' && i+1<len) i++; i++; }
            if (i<len) i++;
            /* distinguish keys (followed by ':') */
            size_t j=i; while (j<len && (t[j]==' '||t[j]=='\t')) j++;
            LexTok k = (j<len && t[j]==':') ? TK_KEYWORD : TK_STRING;
            emit(&e, start, i, k); continue;
        }
        if (isdigit((unsigned char)c) || c=='-') {
            /* allow -, digits, ., e/E, + */
            while (i<len && (isalnum((unsigned char)t[i]) || t[i]=='.' || t[i]=='-' || t[i]=='+')) i++;
            emit(&e, start, i, TK_NUMBER); continue;
        }
        if (c=='t'||c=='f'||c=='n') { /* true/false/null */
            while (i<len && isalpha((unsigned char)t[i])) i++;
            emit(&e, start, i, TK_TYPE); continue;
        }
        if (strchr("{}[],:", c)) { i++; emit(&e, start, i, TK_PUNCT); continue; }
        i++; emit(&e, start, i, TK_TEXT);
    }
    return e.n;
}

/* ---- registry -------------------------------------------------------- */
static size_t unknown_lex(const char *t, size_t len, LexSpan *out, size_t cap) {
    (void)t;
    Emit e = { out, cap, 0 };
    emit(&e, 0, len, TK_TEXT);
    return e.n;
}

/* ---- fold regions (brace-based, language-agnostic) ------------------- */
static int in_literal(const char *t, size_t i) {
    int str = 0, chr = 0, lc = 0, bc = 0;
    for (size_t q = 0; q < i; q++) {
        char c = t[q], n = (q + 1 < i) ? t[q + 1] : 0;
        if (str)      { if (c == '\\' && n) { q++; } else if (c == '"') str = 0; }
        else if (chr) { if (c == '\\' && n) { q++; } else if (c == '\'') chr = 0; }
        else if (bc)  { if (c == '*' && n == '/') bc = 0; }
        else if (lc)  { if (c == '\n') lc = 0; }
        else {
            if (c == '"') str = 1;
            else if (c == '\'') chr = 1;
            else if (c == '/' && n == '*') bc = 1;
            else if (c == '/' && n == '/') lc = 1;
        }
    }
    return str || chr || bc || lc;
}

size_t lex_folds(const char *text, size_t len, LexFold *out, size_t cap) {
    size_t n = 0, sp = 0, stack[256], line = 0;
    for (size_t i = 0; i < len; i++) {
        char c = text[i];
        if (c == '\n') { line++; continue; }
        if (in_literal(text, i)) continue;
        if (c == '{') { if (sp < 256) stack[sp++] = line; }
        else if (c == '}' && sp > 0) {
            size_t a = stack[--sp];
            if (line > a && n < cap) { out[n].start = a; out[n].end = line; n++; }
        }
    }
    return n;
}

/* ---- top-level symbols (C function/definition list) ----------------- */
static size_t lex_line_of(const char *t, size_t off) {
    size_t ln = 0;
    for (size_t q = 0; q < off; q++) if (t[q] == '\n') ln++;
    return ln;
}

size_t lex_symbols(const char *text, size_t len, LexSym *out, size_t cap) {
    size_t n = 0, line = 0, i = 0;
    while (i < len && n < cap) {
        if (!isalpha((unsigned char)text[i]) && text[i] != '_') {
            if (text[i] == '\n') line++;
            i++;
            continue;
        }
        size_t w0 = i, c0 = 0;
        for (size_t q = w0; q > 0 && text[q-1] != '\n'; q--) c0++;
        size_t w1 = i;
        while (w1 < len && (isalnum((unsigned char)text[w1]) || text[w1] == '_')) w1++;
        size_t wlen = w1 - w0;
        int is_kw = kw_match(text + w0, wlen, c_keywords, sizeof c_keywords/sizeof *c_keywords);
        if (is_kw) { i = w1; continue; }   /* control-flow keywords aren't defs */
        size_t j = w1;
        while (j < len && (text[j]==' '||text[j]=='\t')) j++;
        if (j < len && text[j] == '(') {
            size_t p = j, bal = 0;
            while (p < len) {
                if (text[p] == '(') bal++;
                else if (text[p] == ')') { bal--; if (bal == 0) { p++; break; } }
                else if (text[p] == '\n') line++;
                p++;
            }
            while (p < len && (text[p]==' '||text[p]=='\t'||text[p]=='\n')) { if (text[p]=='\n') line++; p++; }
            if (p < len && text[p] == '{') {
                out[n].line = lex_line_of(text, w0);
                out[n].col = c0;
                out[n].name_off = w0;
                out[n].name_len = wlen;
                out[n].kind = is_kw ? TK_KEYWORD : TK_TYPE;
                n++; i = p + 1; continue;
            }
        }
        i = w1;
    }
    return n;
}

/* ---- registry -------------------------------------------------------- */
Lex *lex_create(const char *lang) {
    if (!lang) return NULL;
    Lex *l = malloc(sizeof *l);
    if (!l) return NULL;
    memset(l, 0, sizeof *l);
    strncpy(l->lang, lang, sizeof l->lang - 1);
    if (strcmp(lang, "c") == 0 || strcmp(lang, "cpp") == 0 || strcmp(lang, "h") == 0)
        l->fn = lex_c;
    else if (strcmp(lang, "json") == 0)
        l->fn = lex_json;
    else
        l->fn = unknown_lex;   /* plain text: one span */
    return l;
}

void lex_free(Lex *l) { free(l); }

size_t lex_run(Lex *l, const char *text, size_t len, LexSpan *out, size_t cap) {
    if (!l || !l->fn) return 0;
    return l->fn(text, len, out, cap);
}

const char *lex_lang(const Lex *l) { return l ? l->lang : ""; }
