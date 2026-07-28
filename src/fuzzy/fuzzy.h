/* fuzzy.h -- Atom-style fuzzy finder (subsequence scorer).
 *
 * Core algorithm: test whether `query` is a subsequence of `cand` and score
 * the match by contiguity + word-boundary bonuses (same spirit as Atom's
 * fuzzy-find / fzf). Pure, deterministic, no allocations in the hot path.
 * Used by the command palette and the file/symbol finders. Opaque-free
 * (stateless scoring) so it is trivially testable. */
#ifndef WUBUPAD_FUZZY_H
#define WUBUPAD_FUZZY_H

#include <stddef.h>

/* Score `query` against `cand` (case-insensitive subsequence match).
 * Returns a score >= 0 on match, or -1 if `query` is NOT a subsequence of
 * `cand`. Higher is better. Bonuses: consecutive matches, matches at start
 * of string or after a separator ('/', '.', '_', '-', ' '), and full-word
 * coverage. */
long fuzzy_score(const char *query, const char *cand);

/* True if `query` is a subsequence of `cand` (ignoring case). */
int  fuzzy_match(const char *query, const char *cand);

/* Find the top `cap` candidates (by score) among `n` items. Writes up to
 * `cap` indices (into `items`) into `out_idx` (best first) and their scores
 * into `out_score`. Returns the number written. `cap` must be > 0. */
size_t fuzzy_top(const char **items, size_t n, const char *query,
                 size_t *out_idx, long *out_score, size_t cap);

#endif /* WUBUPAD_FUZZY_H */
