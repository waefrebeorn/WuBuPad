/* docs.h -- multi-document session (the "tabs" model).
 *
 * Owns an ordered set of open documents, an active index, and per-doc
 * metadata (path, dirty flag, encoding, language). Reuses the Doc editing core
 * (no second editing implementation). Opaque. Clean C11. */
#ifndef WUBUPAD_DOCS_H
#define WUBUPAD_DOCS_H

#include <stddef.h>
#include "doc.h"

typedef struct Docs Docs;

Docs *docs_create(void);
void  docs_free(Docs *s);

/* Open a new document (empty or seeded). Returns its index, or SIZE_MAX on
 * error. `path` may be NULL (untitled). `lang` selects the lexer. */
size_t docs_open(Docs *s, const char *path, const char *text, const char *lang);

/* Close document at index `i`. Returns 1 if closed, 0 if invalid. */
int docs_close(Docs *s, size_t i);

size_t docs_count(const Docs *s);
size_t docs_active(const Docs *s);
void   docs_set_active(Docs *s, size_t i);

/* Access the editing core of document `i` (read/write through it). */
Doc   *docs_doc(Docs *s, size_t i);

/* Per-document metadata (pointers valid until the doc is closed). */
const char *docs_path(const Docs *s, size_t i);
const char *docs_lang(const Docs *s, size_t i);
int        docs_dirty(const Docs *s, size_t i);

/* Mark a document clean/dirty (e.g. after save). */
void docs_set_dirty(Docs *s, size_t i, int dirty);

/* Rename / set path after a Save As. */
void docs_set_path(Docs *s, size_t i, const char *path);

/* Load a file from disk into a new document, detecting encoding and seeding
 * the Doc. Returns the doc index, or SIZE_MAX on read error. */
size_t docs_load_file(Docs *s, const char *path, const char *lang);

/* Save document `i` to its path as UTF-8 (no BOM). Returns 0 on success. */
int docs_save_file(Docs *s, size_t i);

#endif /* WUBUPAD_DOCS_H */
