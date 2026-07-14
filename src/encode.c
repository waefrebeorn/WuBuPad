/* encode.c -- encoding detect + convert to UTF-8. Self-contained C11. */
#include "encode.h"

#include <stdlib.h>
#include <string.h>

const char *enc_name(EncKind k) {
    switch (k) {
        case ENC_UTF8:     return "utf-8";
        case ENC_UTF8_BOM: return "utf-8-bom";
        case ENC_UTF16LE:  return "utf-16le";
        case ENC_UTF16BE:  return "utf-16be";
        case ENC_UTF32LE:  return "utf-32le";
        case ENC_UTF32BE:  return "utf-32be";
        case ENC_LATIN1:   return "latin1";
        default:           return "unknown";
    }
}

static int is_valid_utf8(const unsigned char *d, size_t n) {
    size_t i = 0;
    while (i < n) {
        unsigned char c = d[i];
        if (c < 0x80) { i++; }
        else if ((c & 0xE0) == 0xC0) {            /* 110xxxxx, 2 bytes */
            if (i + 1 >= n || (d[i+1] & 0xC0) != 0x80) return 0;
            i += 2;
        } else if ((c & 0xF0) == 0xE0) {         /* 1110xxxx, 3 bytes */
            if (i + 2 >= n || (d[i+1] & 0xC0) != 0x80 || (d[i+2] & 0xC0) != 0x80) return 0;
            i += 3;
        } else if ((c & 0xF8) == 0xF0) {         /* 11110xxx, 4 bytes */
            if (i + 3 >= n || (d[i+1] & 0xC0) != 0x80 || (d[i+2] & 0xC0) != 0x80 ||
                (d[i+3] & 0xC0) != 0x80) return 0;
            i += 4;
        } else return 0;                          /* invalid lead byte */
    }
    return 1;
}

EncKind enc_detect(const unsigned char *data, size_t len) {
    if (len >= 3 && data[0] == 0xEF && data[1] == 0xBB && data[2] == 0xBF)
        return ENC_UTF8_BOM;
    if (len >= 2 && data[0] == 0xFF && data[1] == 0xFE) return ENC_UTF16LE;
    if (len >= 2 && data[0] == 0xFE && data[1] == 0xFF) return ENC_UTF16BE;
    if (len >= 4 && data[0] == 0xFF && data[1] == 0xFE && data[2] == 0x00 && data[3] == 0x00)
        return ENC_UTF32LE;
    if (len >= 4 && data[0] == 0x00 && data[1] == 0x00 && data[2] == 0xFE && data[3] == 0xFF)
        return ENC_UTF32BE;
    /* heuristic: UTF-16 without BOM is rare; if every other byte is NUL and
     * there's at least one non-NUL even byte, guess UTF-16LE. Otherwise UTF-8
     * if valid, else Latin1. */
    if (is_valid_utf8(data, len)) return ENC_UTF8;
    return ENC_LATIN1;
}

/* dynamic UTF-8 builder */
typedef struct { unsigned char *p; size_t n, cap; } U8;

static void u8_init(U8 *u) { u->p = malloc(32); u->n = 0; u->cap = u->p ? 32 : 0; }
static void u8_put(U8 *u, unsigned c) {
    /* encode code point c (<= 0x10FFFF) as UTF-8 */
    if (c < 0x80) {
        if (u->n + 1 > u->cap) { u->cap = u->cap ? u->cap*2 : 32; u->p = realloc(u->p, u->cap); }
        u->p[u->n++] = (unsigned char)c;
    } else if (c < 0x800) {
        if (u->n + 2 > u->cap) { u->cap = (u->cap?u->cap*2:32); u->p = realloc(u->p, u->cap); }
        u->p[u->n++] = 0xC0 | (c >> 6);
        u->p[u->n++] = 0x80 | (c & 0x3F);
    } else if (c < 0x10000) {
        if (u->n + 3 > u->cap) { u->cap = (u->cap?u->cap*2:32); u->p = realloc(u->p, u->cap); }
        u->p[u->n++] = 0xE0 | (c >> 12);
        u->p[u->n++] = 0x80 | ((c >> 6) & 0x3F);
        u->p[u->n++] = 0x80 | (c & 0x3F);
    } else {
        if (u->n + 4 > u->cap) { u->cap = (u->cap?u->cap*2:32); u->p = realloc(u->p, u->cap); }
        u->p[u->n++] = 0xF0 | (c >> 18);
        u->p[u->n++] = 0x80 | ((c >> 12) & 0x3F);
        u->p[u->n++] = 0x80 | ((c >> 6) & 0x3F);
        u->p[u->n++] = 0x80 | (c & 0x3F);
    }
}
char *enc_to_utf8(const unsigned char *data, size_t len, EncKind enc, size_t *out_len) {
    U8 u; u8_init(&u);
    size_t i = 0;

    /* skip a BOM we already accounted for in detection */
    if (enc == ENC_UTF8_BOM) i = 3;

    switch (enc) {
        case ENC_UTF8:
        case ENC_UTF8_BOM:
            while (i < len) {
                if (u.n + 1 > u.cap) { u.cap = u.cap ? u.cap * 2 : 32; u.p = realloc(u.p, u.cap); }
                u.p[u.n++] = data[i++];   /* already UTF-8 bytes */
            }
            break;
        case ENC_UTF16LE:
            while (i + 1 < len) {
                unsigned c = data[i] | (data[i+1] << 8); i += 2;
                u8_put(&u, c);
            }
            break;
        case ENC_UTF16BE:
            while (i + 1 < len) {
                unsigned c = (data[i] << 8) | data[i+1]; i += 2;
                u8_put(&u, c);
            }
            break;
        case ENC_UTF32LE:
            while (i + 3 < len) {
                unsigned c = data[i] | (data[i+1]<<8) | (data[i+2]<<16) | ((unsigned)data[i+3]<<24);
                i += 4; u8_put(&u, c);
            }
            break;
        case ENC_UTF32BE:
            while (i + 3 < len) {
                unsigned c = ((unsigned)data[i]<<24) | (data[i+1]<<16) | (data[i+2]<<8) | data[i+3];
                i += 4; u8_put(&u, c);
            }
            break;
        case ENC_LATIN1:
            while (i < len) u8_put(&u, data[i++]);
            break;
        default:
            while (i < len) u8_put(&u, data[i++]);
            break;
    }
    /* NUL terminate */
    if (u.n + 1 > u.cap) { u.cap = u.n + 1; u.p = realloc(u.p, u.cap); }
    u.p[u.n] = '\0';
    if (out_len) *out_len = u.n;
    return (char *)u.p;
}

char *enc_decode(const unsigned char *data, size_t len, size_t *out_len) {
    return enc_to_utf8(data, len, enc_detect(data, len), out_len);
}
