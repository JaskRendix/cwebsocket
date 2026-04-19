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

/* Helper: fully initialize callback struct */
static struct wsStreamCallbacks make_cbs(void) {
  struct wsStreamCallbacks cbs = {.on_begin = on_begin,
                                  .on_data = on_data,
                                  .on_end = on_end,
                                  .user = NULL,
                                  ._opcode = 0,
                                  ._in_progress = 0};
  return cbs;
}

/* ---- TEST 1: Unknown opcode with FIN=0 ---- */
static void test_unknown_opcode_fin_zero(void) {
  uint8_t f[32];
  size_t n = make_frame(f, 0, 0x0B, (const uint8_t *)"X", 1);

  struct wsStreamCallbacks cbs = make_cbs();

  enum wsFrameType t = wsParseInputFrameStream(f, n, &cbs);
  assert(t == WS_ERROR_FRAME);
}

/* ---- TEST 2: Oversized continuation payload ---- */
static void test_continuation_payload_too_large(void) {
  uint8_t f1[32], f2[512];
  uint8_t payload[300];
  memset(payload, 'A', sizeof(payload));

  size_t n1 = make_frame(f1, 0, WS_TEXT_FRAME, (const uint8_t *)"A", 1);
  size_t n2 = make_frame(f2, 0, 0x00, payload, sizeof(payload));

  struct wsStreamCallbacks cbs = make_cbs();

  enum wsFrameType t1 = wsParseInputFrameStream(f1, n1, &cbs);
  assert(t1 == WS_TEXT_FRAME);

  enum wsFrameType t2 = wsParseInputFrameStream(f2, n2, &cbs);
  assert(t2 == WS_INCOMPLETE_FRAME);
}

/* ---- TEST 3: Oversized final continuation payload ---- */
static void test_final_continuation_payload_too_large(void) {
  uint8_t f1[32], f2[512];
  uint8_t payload[300];
  memset(payload, 'B', sizeof(payload));

  size_t n1 = make_frame(f1, 0, WS_TEXT_FRAME, (const uint8_t *)"A", 1);
  size_t n2 = make_frame(f2, 1, 0x00, payload, sizeof(payload));

  struct wsStreamCallbacks cbs = make_cbs();

  enum wsFrameType t1 = wsParseInputFrameStream(f1, n1, &cbs);
  assert(t1 == WS_TEXT_FRAME);

  enum wsFrameType t2 = wsParseInputFrameStream(f2, n2, &cbs);
  assert(t2 == WS_TEXT_FRAME);
}

/* ---- TEST 4: CONTINUATION with FIN=1 but no prior fragment ---- */
static void test_continuation_fin1_no_start(void) {
  uint8_t f[32];
  size_t n = make_frame(f, 1, 0x00, (const uint8_t *)"X", 1);

  struct wsStreamCallbacks cbs = make_cbs();

  enum wsFrameType t = wsParseInputFrameStream(f, n, &cbs);
  assert(t == WS_ERROR_FRAME);
}

/* ---- TEST 5: TEXT start with FIN=0 and zero payload ---- */
static void test_zero_length_fragment_start(void) {
  uint8_t f1[32], f2[32];

  size_t n1 = make_frame(f1, 0, WS_TEXT_FRAME, NULL, 0);
  size_t n2 = make_frame(f2, 1, 0x00, (const uint8_t *)"A", 1);

  struct wsStreamCallbacks cbs = make_cbs();

  enum wsFrameType t1 = wsParseInputFrameStream(f1, n1, &cbs);
  assert(t1 == WS_TEXT_FRAME);

  enum wsFrameType t2 = wsParseInputFrameStream(f2, n2, &cbs);
  assert(t2 == WS_TEXT_FRAME);
}

/* ---- TEST 6: CONTINUATION with FIN=0 and zero payload (legal) ---- */
static void test_zero_length_continuation_mid(void) {
  uint8_t f1[32], f2[32], f3[32];

  size_t n1 = make_frame(f1, 0, WS_TEXT_FRAME, (const uint8_t *)"A", 1);
  size_t n2 = make_frame(f2, 0, 0x00, NULL, 0);
  size_t n3 = make_frame(f3, 1, 0x00, (const uint8_t *)"B", 1);

  struct wsStreamCallbacks cbs = make_cbs();

  enum wsFrameType t1 = wsParseInputFrameStream(f1, n1, &cbs);
  assert(t1 == WS_TEXT_FRAME);

  enum wsFrameType t2 = wsParseInputFrameStream(f2, n2, &cbs);
  assert(t2 == WS_INCOMPLETE_FRAME);

  enum wsFrameType t3 = wsParseInputFrameStream(f3, n3, &cbs);
  assert(t3 == WS_TEXT_FRAME);
}

/* ---- MAIN ---- */
int main(void) {
  printf("Running extra invalid streaming tests...\n");

  test_unknown_opcode_fin_zero();
  test_continuation_payload_too_large();
  test_final_continuation_payload_too_large();
  test_continuation_fin1_no_start();
  test_zero_length_fragment_start();
  test_zero_length_continuation_mid();

  printf("All extra invalid streaming tests passed.\n");
  return 0;
}
