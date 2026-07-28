/* mdpreview.h -- Markdown -> HTML preview (Atom "markdown-preview" package).
 *
 * A real, dependency-free Markdown subset renderer: headings (#..######),
 * bold (**x**), italic (*x*), inline code (`x`), fenced code blocks (```),
 * unordered/ordered lists, blockquotes (>), horizontal rules (---), and
 * paragraphs / line breaks. Emits a complete, well-formed HTML fragment the
 * UI can render or write to a file. Opaque over a string buffer; testable. C11. */
#ifndef WUBUPAD_MDPREVIEW_H
#define WUBUPAD_MDPREVIEW_H

#include <stddef.h>

/* Render `md` (NUL-terminated Markdown) into a malloc'd NUL-terminated HTML
 * string (caller frees). Returns NULL on OOM. The output is a full document
 * (<!doctype html>...). `title` may be NULL. */
char *mdpreview_render(const char *md, const char *title);

/* Render `md` into `out` (capacity `cap`), returning bytes written (excl. NUL)
 * or (size_t)-1 if truncated. Convenience for fixed buffers / tests. */
size_t mdpreview_render_buf(const char *md, const char *title, char *out, size_t cap);

#endif /* WUBUPAD_MDPREVIEW_H */
