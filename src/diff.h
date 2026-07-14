/* diff.h -- Myers diff (line-based) for compare/document-history.
 *
 * Computes an edit script (equal/insert/delete) between two arrays of lines
 * using the O(ND) algorithm. Opaque; self-contained C11. */
#ifndef WUBUPAD_DIFF_H
#define WUBUPAD_DIFF_H

#include <stddef.h>

typedef enum { DIFF_EQ, DIFF_DEL, DIFF_INS } DiffOp;

typedef struct {
    DiffOp op;
    size_t a_idx;   /* line index in A (for EQ/DEL) */
    size_t b_idx;   /* line index in B (for EQ/INS) */
} DiffEdit;

typedef struct Diff Diff;

/* Build a diff between A[0,na) and B[0,nb). Each element is a NUL-terminated
 * line (no newline). Returns NULL on OOM. Free with diff_free. */
Diff *diff_lines(const char *const *a, size_t na, const char *const *b, size_t nb);

/* Number of edit records. */
size_t diff_count(const Diff *d);
/* Edit record i (0-based). */
const DiffEdit *diff_get(const Diff *d, size_t i);

/* Convenience: returns a malloc'd unified-diff text for human viewing
 * (caller frees), or NULL. */
char *diff_unified(const Diff *d, const char *const *a, const char *const *b,
                   const char *name_a, const char *name_b);

void diff_free(Diff *d);

#endif /* WUBUPAD_DIFF_H */
