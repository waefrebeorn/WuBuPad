/* buffer.h -- gap-free text storage via a piece table.
 *
 * Opaque. The piece table keeps the original text immutable and appends
 * inserted runs to a side "add" buffer; the logical text is the concatenation
 * of pieces. This gives O(1)-ish edit at any position with full undo support
 * and no shifting of the whole document (the property that makes editors
 * handle large files). Clean-room C11. */
#ifndef WUBUPAD_BUFFER_H
#define WUBUPAD_BUFFER_H

#include <stddef.h>

typedef struct Buf Buf;

/* Create a buffer seeded with `text` (may be NULL for an empty buffer). */
Buf *buf_create(const char *text);
void buf_free(Buf *b);

size_t buf_length(const Buf *b);

/* Insert `len` bytes of `text` at document position `pos` (clamped to end). */
void buf_insert(Buf *b, size_t pos, const char *text, size_t len);
/* Delete `len` bytes starting at `pos` (clamped to document bounds). */
void buf_delete(Buf *b, size_t pos, size_t len);

/* Byte at position `pos` (0 if out of range). */
char buf_char_at(const Buf *b, size_t pos);

/* Collect the full text. `out` must hold buf_length(b)+1 bytes; returns len. */
size_t buf_collect(const Buf *b, char *out);
/* Collect into a malloc'd NUL-terminated string (caller frees). */
char *buf_to_string(const Buf *b);

/* Number of lines (newline count, +1 if any content / trailing line). */
size_t buf_line_count(const Buf *b);

#endif /* WUBUPAD_BUFFER_H */
