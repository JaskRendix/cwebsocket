#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "../lib/websocket.h"

/* Utility: build server-side unmasked frame */
static size_t make_frame(uint8_t *out, uint8_t fin, uint8_t opcode,
                         const uint8_t *payload, size_t len) {
  static const uint8_t mask[4] = {0, 0, 0, 0};
  size_t p = 0;

  out[p++] = (fin ? 0x80 : 0x00) | opcode;

  if (len <= 125) {
    out[p++] = 0x80 | (uint8_t)len;
  } else if (len <= 0xFFFF) {
    out[p++] = 0x80 | 126;
    out[p++] = (uint8_t)(len >> 8);
    out[p++] = (uint8_t)(len & 0xFF);
  } else {
    out[p++] = 0x80 | 127;
    uint64_t v = len;
    for (int i = 7; i >= 0; i--)
      out[p++] = (uint8_t)((v >> (i * 8)) & 0xFF);
  }

  memcpy(&out[p], mask, 4);
  p += 4;

  if (payload && len > 0)
    memcpy(&out[p], payload, len);

  return p + len;
}

/* ---- TEST 1: Control frame with FIN=0 → ERROR ---- */
static void test_control_fin_zero(void) {
  uint8_t f[32];
  size_t n = make_frame(f, 0, WS_PING_FRAME, (const uint8_t *)"X", 1);

  uint8_t buf[32];
  struct wsMessageContext ctx;
  wsInitMessageContext(&ctx, buf, sizeof(buf));

  uint8_t *data = NULL;
  size_t len = 0;

  enum wsFrameType t = wsParseInputFrameWithContext(f, n, &data, &len, &ctx);
  assert(t == WS_ERROR_FRAME);
}

/* ---- TEST 2: Control frame payload >125 → ERROR ---- */
static void test_control_payload_too_large(void) {
  uint8_t payload[200];
  memset(payload, 'A', sizeof(payload));

  uint8_t f[512];
  size_t n = make_frame(f, 1, WS_PING_FRAME, payload, sizeof(payload));

  uint8_t buf[32];
  struct wsMessageContext ctx;
  wsInitMessageContext(&ctx, buf, sizeof(buf));

  uint8_t *data = NULL;
  size_t len = 0;

  enum wsFrameType t = wsParseInputFrameWithContext(f, n, &data, &len, &ctx);
  assert(t == WS_ERROR_FRAME);
}

/* ---- TEST 3: Oversized fragmented message → ERROR ---- */
static void test_fragment_overflow(void) {
  uint8_t f1[64], f2[512];
  uint8_t payload[300];
  memset(payload, 'B', sizeof(payload));

  size_t n1 = make_frame(f1, 0, WS_TEXT_FRAME, (const uint8_t *)"A", 1);
  size_t n2 = make_frame(f2, 1, 0x00, payload, sizeof(payload));

  uint8_t buf[32];
  struct wsMessageContext ctx;
  wsInitMessageContext(&ctx, buf, sizeof(buf));

  uint8_t *data = NULL;
  size_t len = 0;

  enum wsFrameType t1 = wsParseInputFrameWithContext(f1, n1, &data, &len, &ctx);
  assert(t1 == WS_INCOMPLETE_FRAME);

  enum wsFrameType t2 = wsParseInputFrameWithContext(f2, n2, &data, &len, &ctx);
  assert(t2 == WS_ERROR_FRAME);
}

/* ---- TEST 4: CONTINUATION with FIN=1 but no start → ERROR ---- */
static void test_continuation_fin1_no_start(void) {
  uint8_t f[32];
  size_t n = make_frame(f, 1, 0x00, (const uint8_t *)"X", 1);

  uint8_t buf[32];
  struct wsMessageContext ctx;
  wsInitMessageContext(&ctx, buf, sizeof(buf));

  uint8_t *data = NULL;
  size_t len = 0;

  enum wsFrameType t = wsParseInputFrameWithContext(f, n, &data, &len, &ctx);
  assert(t == WS_ERROR_FRAME);
}

/* ---- TEST 5: CONTINUATION with FIN=0 and zero payload (legal) ---- */
static void test_zero_length_continuation_mid(void) {
  uint8_t f1[32], f2[32], f3[32];

  size_t n1 = make_frame(f1, 0, WS_TEXT_FRAME, (const uint8_t *)"A", 1);
  size_t n2 = make_frame(f2, 0, 0x00, NULL, 0);
  size_t n3 = make_frame(f3, 1, 0x00, (const uint8_t *)"B", 1);

  uint8_t buf[32];
  struct wsMessageContext ctx;
  wsInitMessageContext(&ctx, buf, sizeof(buf));

  uint8_t *data = NULL;
  size_t len = 0;

  enum wsFrameType t1 = wsParseInputFrameWithContext(f1, n1, &data, &len, &ctx);
  assert(t1 == WS_INCOMPLETE_FRAME);

  enum wsFrameType t2 = wsParseInputFrameWithContext(f2, n2, &data, &len, &ctx);
  assert(t2 == WS_INCOMPLETE_FRAME);

  enum wsFrameType t3 = wsParseInputFrameWithContext(f3, n3, &data, &len, &ctx);
  assert(t3 == WS_TEXT_FRAME);
}

/* ---- TEST 6: TEXT start with FIN=0 and zero payload ---- */
static void test_zero_length_fragment_start(void) {
  uint8_t f1[32], f2[32];

  size_t n1 = make_frame(f1, 0, WS_TEXT_FRAME, NULL, 0);
  size_t n2 = make_frame(f2, 1, 0x00, (const uint8_t *)"A", 1);

  uint8_t buf[32];
  struct wsMessageContext ctx;
  wsInitMessageContext(&ctx, buf, sizeof(buf));

  uint8_t *data = NULL;
  size_t len = 0;

  enum wsFrameType t1 = wsParseInputFrameWithContext(f1, n1, &data, &len, &ctx);
  assert(t1 == WS_INCOMPLETE_FRAME);

  enum wsFrameType t2 = wsParseInputFrameWithContext(f2, n2, &data, &len, &ctx);
  assert(t2 == WS_TEXT_FRAME);
}

/* ---- MAIN ---- */
int main(void) {
  printf("Running extra continuation invalid tests...\n");

  test_control_fin_zero();
  test_control_payload_too_large();
  test_fragment_overflow();
  test_continuation_fin1_no_start();
  test_zero_length_continuation_mid();
  test_zero_length_fragment_start();

  printf("All extra continuation invalid tests passed.\n");
  return 0;
}
