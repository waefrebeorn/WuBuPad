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

typedef struct Lex Lex;

/* Registry: create a lexer for a named language ("c", "json", ...).
 * Returns NULL if the language is unknown. */
Lex *lex_create(const char *lang);
void lex_free(Lex *l);

/* Lex the `text` (length `len`) into `out` spans (capacity `cap`).
 * Returns the number of spans emitted (may be < cap if truncated). */
size_t lex_run(Lex *l, const char *text, size_t len, LexSpan *out, size_t cap);

const char *lex_lang(const Lex *l);

#endif /* WUBUPAD_LEX_H */
