/*
 * Copyright (c) 2014 Malte Hildingsson, malte (at) afterwi.se
 *
 * MIT License
 */

#ifndef AW_SHA1_H
#define AW_SHA1_H

#include <stddef.h>
#include <stdint.h>
#include <string.h>

#if _MSC_VER
#define _sha1_restrict __restrict
#else
#define _sha1_restrict __restrict__
#endif

#define SHA1_SIZE 20

#ifdef __cplusplus
extern "C" {
#endif

static inline uint32_t sha1_rol(uint32_t x, uint32_t s) {
  return (uint32_t)((x << s) | (x >> (32U - s)));
}

static inline void sha1mix(uint32_t *_sha1_restrict r,
                           const uint32_t *_sha1_restrict w) {
  uint32_t a = r[0];
  uint32_t b = r[1];
  uint32_t c = r[2];
  uint32_t d = r[3];
  uint32_t e = r[4];

  uint32_t t;
  uint32_t i;

  /* Round 0-19 */
  for (i = 0; i < 20; ++i) {
    t = sha1_rol(a, 5) + ((b & c) | ((~b) & d)) + e + w[i] + 0x5a827999U;
    e = d;
    d = c;
    c = sha1_rol(b, 30);
    b = a;
    a = t;
  }

  /* Round 20-39 */
  for (; i < 40; ++i) {
    t = sha1_rol(a, 5) + (b ^ c ^ d) + e + w[i] + 0x6ed9eba1U;
    e = d;
    d = c;
    c = sha1_rol(b, 30);
    b = a;
    a = t;
  }

  /* Round 40-59 */
  for (; i < 60; ++i) {
    t = sha1_rol(a, 5) + ((b & c) | (b & d) | (c & d)) + e + w[i] + 0x8f1bbcdcU;
    e = d;
    d = c;
    c = sha1_rol(b, 30);
    b = a;
    a = t;
  }

  /* Round 60-79 */
  for (; i < 80; ++i) {
    t = sha1_rol(a, 5) + (b ^ c ^ d) + e + w[i] + 0xca62c1d6U;
    e = d;
    d = c;
    c = sha1_rol(b, 30);
    b = a;
    a = t;
  }

  r[0] += a;
  r[1] += b;
  r[2] += c;
  r[3] += d;
  r[4] += e;
}

/*
 * Drop-in compatible API:
 *   - h: output buffer of size SHA1_SIZE
 *   - p: input data pointer
 *   - n: input length in bytes
 */
static inline void sha1(unsigned char h[static SHA1_SIZE],
                        const void *_sha1_restrict p, size_t n) {
  const uint8_t *data = (const uint8_t *)p;
  uint32_t w[80];
  uint32_t r[5] = {0x67452301U, 0xefcdab89U, 0x98badcfeU, 0x10325476U,
                   0xc3d2e1f0U};

  size_t i = 0;

  /* Process full 64-byte blocks */
  while (n - i >= 64U) {
    size_t j;

    for (j = 0; j < 16; ++j) {
      size_t idx = i + (j * 4U);
      w[j] = ((uint32_t)data[idx + 0] << 24) | ((uint32_t)data[idx + 1] << 16) |
             ((uint32_t)data[idx + 2] << 8) | ((uint32_t)data[idx + 3] << 0);
    }

    for (j = 16; j < 80; ++j) {
      w[j] = sha1_rol(w[j - 3] ^ w[j - 8] ^ w[j - 14] ^ w[j - 16], 1);
    }

    sha1mix(r, w);
    i += 64U;
  }

  /* Build final block(s) with padding */
  {
    uint8_t block[64];
    size_t rem = n - i;
    size_t j;

    memset(block, 0, sizeof block);

    /* Copy remaining bytes */
    for (j = 0; j < rem; ++j) {
      block[j] = data[i + j];
    }

    /* Append 0x80 */
    block[rem] = 0x80U;

    /* If not enough room for length, process this block and use another */
    if (rem >= 56U) {
      for (j = 0; j < 16; ++j) {
        size_t idx = j * 4U;
        w[j] = ((uint32_t)block[idx + 0] << 24) |
               ((uint32_t)block[idx + 1] << 16) |
               ((uint32_t)block[idx + 2] << 8) |
               ((uint32_t)block[idx + 3] << 0);
      }

      for (j = 16; j < 80; ++j) {
        w[j] = sha1_rol(w[j - 3] ^ w[j - 8] ^ w[j - 14] ^ w[j - 16], 1);
      }

      sha1mix(r, w);
      memset(block, 0, sizeof block);
    }

    /* Append total length in bits (big-endian) */
    {
      uint64_t bit_len = (uint64_t)n * 8U;
      block[56] = (uint8_t)((bit_len >> 56) & 0xffU);
      block[57] = (uint8_t)((bit_len >> 48) & 0xffU);
      block[58] = (uint8_t)((bit_len >> 40) & 0xffU);
      block[59] = (uint8_t)((bit_len >> 32) & 0xffU);
      block[60] = (uint8_t)((bit_len >> 24) & 0xffU);
      block[61] = (uint8_t)((bit_len >> 16) & 0xffU);
      block[62] = (uint8_t)((bit_len >> 8) & 0xffU);
      block[63] = (uint8_t)((bit_len >> 0) & 0xffU);
    }

    /* Final block */
    for (j = 0; j < 16; ++j) {
      size_t idx = j * 4U;
      w[j] = ((uint32_t)block[idx + 0] << 24) |
             ((uint32_t)block[idx + 1] << 16) |
             ((uint32_t)block[idx + 2] << 8) | ((uint32_t)block[idx + 3] << 0);
    }

    for (j = 16; j < 80; ++j) {
      w[j] = sha1_rol(w[j - 3] ^ w[j - 8] ^ w[j - 14] ^ w[j - 16], 1);
    }

    sha1mix(r, w);
  }

  /* Output digest (big-endian) */
  for (size_t i_out = 0; i_out < 5; ++i_out) {
    h[(i_out * 4U) + 0] = (unsigned char)((r[i_out] >> 24) & 0xffU);
    h[(i_out * 4U) + 1] = (unsigned char)((r[i_out] >> 16) & 0xffU);
    h[(i_out * 4U) + 2] = (unsigned char)((r[i_out] >> 8) & 0xffU);
    h[(i_out * 4U) + 3] = (unsigned char)((r[i_out] >> 0) & 0xffU);
  }
}

#ifdef __cplusplus
}
#endif

#endif /* AW_SHA1_H */
