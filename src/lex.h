/* lex.h -- syntax token model + language registry. Opaque to callers. */
#ifndef WUBUPAD_LEX_H
#define WUBUPAD_LEX_H

#include <stddef.h>

typedef enum {
    TK_NONE = 0,
    TK_TEXT,        /* default/plain */
    TK_KEYWORD,
    TK_TYPE,
    TK_STRING,
    TK_CHAR,
    TK_NUMBER,
    TK_COMMENT,
    TK_PREPROC,     /* #directive */
    TK_OPERATOR,
    TK_PUNCT,
    TK_IDENT,
    TK_WHITESPACE,
    TK_ERROR
} LexTok;

/* A token range in the document (byte offsets, half-open). */
typedef struct { size_t start; size_t end; LexTok kind; } LexSpan;

/* A foldable brace region (0-based line numbers). Folding hides the body
 * lines (start+1 .. end-1), keeping the header lines visible. */
typedef struct { size_t start; size_t end; } LexFold;

/* A top-level symbol (function/definition) for the function list.
 * `name_off`/`name_len` index into the source text `t` passed to lex_symbols. */
typedef struct {
    size_t line;       /* 0-based line of the symbol name */
    size_t col;        /* 0-based column */
    size_t name_off;   /* byte offset of the name in `t` */
    size_t name_len;   /* length of the name */
    LexTok kind;       /* TK_TYPE for functions, TK_KEYWORD for macros, etc. */
} LexSym;

typedef struct Lex Lex;

/* Registry: create a lexer for a named language ("c", "json", ...).
 * Returns NULL if the language is unknown. */
Lex *lex_create(const char *lang);
void lex_free(Lex *l);

/* Lex the `text` (length `len`) into `out` spans (capacity `cap`).
 * Returns the number of spans emitted (may be < cap if truncated). */
size_t lex_run(Lex *l, const char *text, size_t len, LexSpan *out, size_t cap);

const char *lex_lang(const Lex *l);

/* Fold regions (brace-based). Fills `out` (capacity `cap`) with foldable
 * blocks; returns the count. Cheap and language-agnostic (skips string/char/
 * comment literals). */
size_t lex_folds(const char *text, size_t len, LexFold *out, size_t cap);

/* Top-level symbols for a function list. Fills `out` (capacity `cap`);
 * returns the count. `name_off`/`name_len` index into `text`. */
size_t lex_symbols(const char *text, size_t len, LexSym *out, size_t cap);

#endif /* WUBUPAD_LEX_H */
