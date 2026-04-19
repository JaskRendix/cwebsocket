#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "../lib/websocket.h"

/* ---- Utility: build server-side unmasked frame ---- */
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

/* ---- Dummy callbacks ---- */
static void on_begin(uint8_t opcode, size_t total, void *u) {
  (void)opcode;
  (void)total;
  (void)u;
}
static void on_data(const uint8_t *d, size_t n, void *u) {
  (void)d;
  (void)n;
  (void)u;
}
static void on_end(void *u) { (void)u; }

/* ---- TEST 1: CONTINUATION without start ---- */
static void test_continuation_without_start(void) {
  uint8_t f[32];
  size_t n = make_frame(f, 1, 0x00, (const uint8_t *)"X", 1);

  struct wsStreamCallbacks cbs = {
      .on_begin = on_begin, .on_data = on_data, .on_end = on_end, .user = NULL};

  enum wsFrameType t = wsParseInputFrameStream(f, n, &cbs);
  assert(t == WS_ERROR_FRAME);
}

/* ---- TEST 2: TEXT during fragmented message ---- */
static void test_text_during_fragment(void) {
  uint8_t f1[32], f2[32];
  size_t n1 = make_frame(f1, 0, WS_TEXT_FRAME, (const uint8_t *)"A", 1);
  size_t n2 = make_frame(f2, 1, WS_TEXT_FRAME, (const uint8_t *)"B", 1);

  struct wsStreamCallbacks cbs = {
      .on_begin = on_begin, .on_data = on_data, .on_end = on_end, .user = NULL};

  enum wsFrameType t1 = wsParseInputFrameStream(f1, n1, &cbs);
  assert(t1 == WS_TEXT_FRAME);

  enum wsFrameType t2 = wsParseInputFrameStream(f2, n2, &cbs);
  assert(t2 == WS_ERROR_FRAME);
}

/* ---- TEST 3: Control frame with FIN=0 ---- */
static void test_control_fin_zero(void) {
  uint8_t f[32];
  size_t n = make_frame(f, 0, WS_PING_FRAME, (const uint8_t *)"X", 1);

  struct wsStreamCallbacks cbs = {
      .on_begin = on_begin, .on_data = on_data, .on_end = on_end, .user = NULL};

  enum wsFrameType t = wsParseInputFrameStream(f, n, &cbs);
  assert(t == WS_ERROR_FRAME);
}

/* ---- TEST 4: Control frame payload >125 ---- */
static void test_control_payload_too_large(void) {
  uint8_t payload[200];
  memset(payload, 'A', sizeof(payload));

  uint8_t f[512];
  size_t n = make_frame(f, 1, WS_PING_FRAME, payload, sizeof(payload));

  struct wsStreamCallbacks cbs = {
      .on_begin = on_begin, .on_data = on_data, .on_end = on_end, .user = NULL};

  enum wsFrameType t = wsParseInputFrameStream(f, n, &cbs);
  assert(t == WS_ERROR_FRAME);
}

/* ---- TEST 5: CONTINUATION after final fragment ---- */
static void test_continuation_after_final(void) {
  uint8_t f1[32], f2[32], f3[32];

  size_t n1 = make_frame(f1, 0, WS_TEXT_FRAME, (const uint8_t *)"A", 1);
  size_t n2 = make_frame(f2, 1, 0x00, (const uint8_t *)"B", 1);
  size_t n3 = make_frame(f3, 1, 0x00, (const uint8_t *)"C", 1);

  struct wsStreamCallbacks cbs = {
      .on_begin = on_begin, .on_data = on_data, .on_end = on_end, .user = NULL};

  enum wsFrameType t1 = wsParseInputFrameStream(f1, n1, &cbs);
  assert(t1 == WS_TEXT_FRAME);

  enum wsFrameType t2 = wsParseInputFrameStream(f2, n2, &cbs);
  assert(t2 == WS_TEXT_FRAME);

  enum wsFrameType t3 = wsParseInputFrameStream(f3, n3, &cbs);
  assert(t3 == WS_ERROR_FRAME);
}

/* ---- TEST 6: Unknown opcode ---- */
static void test_unknown_opcode(void) {
  uint8_t f[32];
  size_t n =
      make_frame(f, 1, 0x0B, (const uint8_t *)"X", 1); /* 0x0B is reserved */

  struct wsStreamCallbacks cbs = {
      .on_begin = on_begin, .on_data = on_data, .on_end = on_end, .user = NULL};

  enum wsFrameType t = wsParseInputFrameStream(f, n, &cbs);
  assert(t == WS_ERROR_FRAME);
}

/* ---- MAIN ---- */
int main(void) {
  printf("Running invalid streaming tests...\n");

  test_continuation_without_start();
  test_text_during_fragment();
  test_control_fin_zero();
  test_control_payload_too_large();
  test_continuation_after_final();
  test_unknown_opcode();

  printf("All invalid streaming tests passed.\n");
  return 0;
}
