/* complete.h -- word completion + symbol/function list extraction.
 *
 * Both operate on a document's text (or any buffer) and the lexer, so they
 * are backend-agnostic and trivially testable. Clean C11. */
#ifndef WUBUPAD_COMPLETE_H
#define WUBUPAD_COMPLETE_H

#include <stddef.h>

/* A symbol occurrence (function name, identifier at a line). */
typedef struct {
    char  *name;     /* malloc'd */
    size_t line;     /* 0-based */
    int    is_func;  /* 1 if followed by '(' (function-like) */
} DocSymbol;

/* Collect every distinct word in `text` (bytes, len) that starts with
 * `prefix` (may be empty). Returns malloc'd array of malloc'd strings in
 * `out` (count in *n). Caller frees with doc_complete_free. */
char **doc_complete(const char *text, size_t len,
                    const char *prefix, size_t *n);

/* Free a list returned by doc_complete. */
void doc_complete_free(char **list, size_t n);

/* Extract symbols (identifiers; is_func set when followed by '(') with their
 * line numbers. Returns malloc'd DocSymbol array in *out (*n count). Caller
 * frees with doc_symbols_free. */
DocSymbol *doc_symbols(const char *text, size_t len, size_t *n);
void doc_symbols_free(DocSymbol *syms, size_t n);

#endif /* WUBUPAD_COMPLETE_H */
