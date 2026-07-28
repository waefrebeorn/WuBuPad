/* autoindent.h -- smart typing / auto-indent (Atom "language" auto-indent).
 *
 * Given context (the line being left + the typed key), computes the new
 * indentation (in spaces) and whether an extra closing line should be
 * inserted. Pure logic over a line buffer so it is headless-testable. Opaque
 * helper over plain strings; the UI applies the result via Doc ops. C11. */
#ifndef WUBUPAD_AUTOINDENT_H
#define WUBUPAD_AUTOINDENT_H

#include <stddef.h>

/* Given the current line's text (NUL-terminated) and the just-typed char
 * (`key` is '\n' for Enter, or '{' / '(' / '[' for openers), compute:
 *   *out_indent   = number of leading spaces the NEW line should have
 *   *out_extra    = 1 if a matching closer line should be auto-inserted below
 *                   (so the caret sits between open/close), else 0.
 * Returns 0 always (no error path); pure. */
void autoindent_on_key(const char *line, char key,
                       int *out_indent, int *out_extra);

/* Continuation indent for a soft-wrapped / hanging line (e.g. after an
 * operator). Returns spaces to add on the next line. */
int autoindent_continued(const char *line);

#endif /* WUBUPAD_AUTOINDENT_H */
