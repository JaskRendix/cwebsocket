
/*
 Copyright (c) 2014 Malte Hildingsson, malte (at) afterwi.se
 
 Permission is hereby granted, free of charge, to any person obtaining a copy
 of this software and associated documentation files (the "Software"), to deal
 in the Software without restriction, including without limitation the rights
 to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 copies of the Software, and to permit persons to whom the Software is
 furnished to do so, subject to the following conditions:
 
 The above copyright notice and this permission notice shall be included in
 all copies or substantial portions of the Software.
 
 THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 THE SOFTWARE.
 */

#ifndef AW_BASE64_H
#define AW_BASE64_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Base64 output length for n input bytes */
static inline size_t base64len(size_t n) {
    return ((n + 2) / 3) * 4;
}

/* Compile-time constant version for static array sizing */
#define BASE64LEN(n) ((((n) + 2) / 3) * 4)

/*
 * Modern, safe Base64 encoder.
 *
 * - buf: output buffer (must be at least base64len(n))
 * - nbuf: size of buf
 * - p: input bytes
 * - n: input length
 *
 * Returns: number of bytes written (same as base64len(n))
 */
static inline size_t base64(char *buf, size_t nbuf,
                            const uint8_t *p, size_t n)
{
    static const char table[64] =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

    const size_t out_len = base64len(n);
    if (nbuf < out_len) {
        return 0; /* not enough space */
    }

    size_t i = 0;
    size_t o = 0;

    while (i < n) {
        uint32_t x = 0;
        size_t rem = n - i;

        /* Pack up to 3 bytes into 24 bits */
        x |= (uint32_t)p[i] << 16;
        if (rem > 1) x |= (uint32_t)p[i + 1] << 8;
        if (rem > 2) x |= (uint32_t)p[i + 2];

        /* Encode 4 Base64 chars */
        buf[o++] = table[(x >> 18) & 0x3F];
        buf[o++] = table[(x >> 12) & 0x3F];
        buf[o++] = (rem > 1) ? table[(x >> 6) & 0x3F] : '=';
        buf[o++] = (rem > 2) ? table[x & 0x3F] : '=';

        i += (rem >= 3) ? 3 : rem;
    }

    return out_len;
}

#ifdef __cplusplus
}
#endif

#endif /* AW_BASE64_H */
