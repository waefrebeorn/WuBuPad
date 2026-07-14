/* encode.h -- text encoding detection and conversion.
 *
 * Detects UTF-8 (with/without BOM), UTF-16 LE/BE, UTF-32 LE/BE, and "other"
 * (treated as single-byte / ANSI). Converts any of these to a normalized UTF-8
 * NUL-terminated string. Opaque; no third-party deps. Clean C11. */
#ifndef WUBUPAD_ENCODE_H
#define WUBUPAD_ENCODE_H

#include <stddef.h>

typedef enum {
    ENC_UNKNOWN = 0,
    ENC_UTF8,
    ENC_UTF8_BOM,
    ENC_UTF16LE,
    ENC_UTF16BE,
    ENC_UTF32LE,
    ENC_UTF32BE,
    ENC_LATIN1        /* single-byte, assumed ISO-8859-1 */
} EncKind;

const char *enc_name(EncKind k);

/* Detect encoding from a byte buffer (may inspect a BOM, else heuristic). */
EncKind enc_detect(const unsigned char *data, size_t len);

/* Convert `data`[0,len) (of detected/forced encoding) to UTF-8.
 * Returns a malloc'd NUL-terminated string (caller frees), or NULL on error.
 * `out_len` (if non-NULL) receives the byte length excluding the NUL. */
char *enc_to_utf8(const unsigned char *data, size_t len, EncKind enc, size_t *out_len);

/* Convenience: detect then convert. */
char *enc_decode(const unsigned char *data, size_t len, size_t *out_len);

#endif /* WUBUPAD_ENCODE_H */
