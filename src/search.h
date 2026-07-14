/* search.h -- text search over a document/string.
 *
 * Finds matches by byte range [start,end). Supports:
 *   - literal substring search
 *   - POSIX-ish regex via a Thompson NFA (Russ Cox construction): concatenation,
 *     alternation (|), grouping (()), quantifiers * + ? and ., char classes
 *     [a-z], and ^ $ anchors. Case-sensitive and case-insensitive modes.
 *
 * Opaque to callers; the NFA is built once and can be matched many times.
 * Clean-room C11. */
#ifndef WUBUPAD_SEARCH_H
#define WUBUPAD_SEARCH_H

#include <stddef.h>

typedef enum { SEARCH_OK = 0, SEARCH_BAD_PATTERN = -1 } SearchStatus;

typedef struct Regex Regex;

/* Compile `pattern`. Returns NULL on a malformed pattern (parenthesis/brace
 * mismatch, empty quantifier target). `icase` enables case-insensitivity. */
Regex *regex_compile(const char *pattern, int icase);
void   regex_free(Regex *r);

/* Find the first match of `re` in `text`[0,len). On success sets *m_start and
 * *m_end (half-open, in bytes) and returns 1. Returns 0 if no match. */
int regex_find(const Regex *re, const char *text, size_t len,
                size_t *m_start, size_t *m_end);

/* Find the next match starting at or after `from` (for "find next"). */
int regex_find_from(const Regex *re, const char *text, size_t len,
                     size_t from, size_t *m_start, size_t *m_end);

/* ---- literal search (no regex) -------------------------------------- */
/* Find `needle`[0,nlen) in `hay`[0,hlen) starting at `from`. Returns the match
 * start byte or SIZE_MAX if not found. */
size_t search_literal(const char *hay, size_t hlen,
                      const char *needle, size_t nlen, size_t from);

#endif /* WUBUPAD_SEARCH_H */
