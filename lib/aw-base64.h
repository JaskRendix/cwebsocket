/*
 * Copyright (c) 2014 Malte Hildingsson, malte (at) afterwi.se
 *
 * MIT License
 */

#ifndef AW_BASE64_H
#define AW_BASE64_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Base64 output length for n input bytes (not including null terminator) */
static inline size_t base64len(size_t n) { return ((n + 2) / 3) * 4; }

/* Compile-time constant version (useful for stack buffers: char
 * buf[BASE64LEN(20) + 1]) */
#define BASE64LEN(n) ((((n) + 2) / 3) * 4)

/**
 * Modern, safe Base64 encoder.
 *
 * - buf: output buffer
 * - nbuf: size of buf (should be at least base64len(n) + 1 for null terminator)
 * - p: input bytes
 * - n: input length
 *
 * Returns: number of bytes written (excluding null terminator), or 0 on error.
 */
static inline size_t base64(char *buf, size_t nbuf, const uint8_t *p,
                            size_t n) {
  static const char table[64] =
      "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

  const size_t out_len = base64len(n);

  /* Ensure buffer can hold the output AND a null terminator */
  if (nbuf < (out_len + 1)) {
    return 0;
  }

  size_t i = 0;
  size_t o = 0;

  for (i = 0; i < n; i += 3) {
    uint32_t x = 0;
    size_t rem = n - i;

    /* Pack bytes into 24-bit buffer */
    x |= (uint32_t)p[i] << 16;
    if (rem > 1)
      x |= (uint32_t)p[i + 1] << 8;
    if (rem > 2)
      x |= (uint32_t)p[i + 2];

    /* Encode 4 chars */
    buf[o++] = table[(x >> 18) & 0x3F];
    buf[o++] = table[(x >> 12) & 0x3F];
    buf[o++] = (rem > 1) ? table[(x >> 6) & 0x3F] : '=';
    buf[o++] = (rem > 2) ? table[x & 0x3F] : '=';
  }

  buf[o] = '\0'; /* Always null-terminate for string safety */
  return o;
}

#ifdef __cplusplus
}
#endif

#endif /* AW_BASE64_H */
