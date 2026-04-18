#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "../lib/websocket.h"

/* ---- Fake send() recorder ---- */

struct send_recorder {
  uint8_t out[4096];
  size_t len;
};

static void fake_send(struct send_recorder *r, const uint8_t *buf, size_t n) {
  assert(r->len + n < sizeof(r->out));
  memcpy(r->out + r->len, buf, n);
  r->len += n;
}

/* ---- Echo context identical to server ---- */

struct echo_ctx {
  struct send_recorder *rec;
  uint8_t outbuf[4096];
};

/* ---- Echo callbacks ---- */

static void on_begin(uint8_t opcode, size_t total, void *u) {
  (void)opcode;
  (void)total;
  (void)u;
}

static void on_data(const uint8_t *data, size_t len, void *u) {
  struct echo_ctx *ctx = (struct echo_ctx *)u;

  size_t out_len = sizeof(ctx->outbuf);
  wsMakeFrame(data, len, ctx->outbuf, &out_len, WS_TEXT_FRAME);

  fake_send(ctx->rec, ctx->outbuf, out_len);
}

static void on_end(void *u) { (void)u; }

/* ---- Utility: build server-side unmasked frame ---- */

static size_t make_frame(uint8_t *out, uint8_t fin, uint8_t opcode,
                         const uint8_t *payload, size_t len) {
  static const uint8_t mask[4] = {0x00, 0x00, 0x00, 0x00};
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
    uint64_t v = (uint64_t)len;
    for (int i = 7; i >= 0; i--)
      out[p++] = (uint8_t)((v >> (i * 8)) & 0xFF);
  }
  memcpy(&out[p], mask, 4);
  p += 4;
  memcpy(&out[p], payload, len); /* all-zero mask = no-op XOR */
  return p + len;
}

/* ---- TEST 1: unfragmented echo ---- */

static void test_unfragmented_echo(void) {
  uint8_t frame[64];
  size_t f = make_frame(frame, 1, WS_TEXT_FRAME, (const uint8_t *)"Hi", 2);

  struct send_recorder rec = {.len = 0};
  struct echo_ctx ctx = {.rec = &rec};

  struct wsStreamCallbacks cbs = {
      .on_begin = on_begin, .on_data = on_data, .on_end = on_end, .user = &ctx};

  wsParseInputFrameStream(frame, f, &cbs);

  /* Expect a TEXT echo frame containing "Hi" */
  uint8_t expected[64];
  size_t e = sizeof(expected);
  wsMakeFrame((const uint8_t *)"Hi", 2, expected, &e, WS_TEXT_FRAME);

  assert(rec.len == e);
  assert(memcmp(rec.out, expected, e) == 0);
}

/* ---- TEST 2: fragmented echo ---- */

static void test_fragmented_echo(void) {
  uint8_t f1[64], f2[64];
  size_t n1 = make_frame(f1, 0, WS_TEXT_FRAME, (const uint8_t *)"Hel", 3);
  size_t n2 = make_frame(f2, 1, 0x00, (const uint8_t *)"lo", 2);

  struct send_recorder rec = {.len = 0};
  struct echo_ctx ctx = {.rec = &rec};

  struct wsStreamCallbacks cbs = {
      .on_begin = on_begin, .on_data = on_data, .on_end = on_end, .user = &ctx};

  wsParseInputFrameStream(f1, n1, &cbs);
  wsParseInputFrameStream(f2, n2, &cbs);

  /* Expect two TEXT frames: "Hel" and "lo" */
  uint8_t e1[64], e2[64];
  size_t e1n = sizeof(e1), e2n = sizeof(e2);

  wsMakeFrame((const uint8_t *)"Hel", 3, e1, &e1n, WS_TEXT_FRAME);
  wsMakeFrame((const uint8_t *)"lo", 2, e2, &e2n, WS_TEXT_FRAME);

  assert(rec.len == e1n + e2n);
  assert(memcmp(rec.out, e1, e1n) == 0);
  assert(memcmp(rec.out + e1n, e2, e2n) == 0);
}

/* ---- TEST 3: binary echo ---- */

static void test_binary_echo(void) {
  uint8_t payload[] = {0x01, 0x02, 0x03};

  uint8_t frame[64];
  size_t f = make_frame(frame, 1, WS_BINARY_FRAME, payload, sizeof(payload));

  struct send_recorder rec = {.len = 0};
  struct echo_ctx ctx = {.rec = &rec};

  struct wsStreamCallbacks cbs = {
      .on_begin = on_begin, .on_data = on_data, .on_end = on_end, .user = &ctx};

  wsParseInputFrameStream(frame, f, &cbs);

  uint8_t expected[64];
  size_t e = sizeof(expected);
  wsMakeFrame(payload, sizeof(payload), expected, &e, WS_TEXT_FRAME);

  assert(rec.len == e);
  assert(memcmp(rec.out, expected, e) == 0);
}

/* ---- TEST 4: CLOSE frame triggers no echo ---- */

static void test_close_no_echo(void) {
  uint8_t frame[64];
  size_t f = make_frame(frame, 1, WS_CLOSING_FRAME, NULL, 0);

  struct send_recorder rec = {.len = 0};
  struct echo_ctx ctx = {.rec = &rec};

  struct wsStreamCallbacks cbs = {
      .on_begin = on_begin, .on_data = on_data, .on_end = on_end, .user = &ctx};

  enum wsFrameType t = wsParseInputFrameStream(frame, f, &cbs);

  assert(t == WS_CLOSING_FRAME);
  assert(rec.len == 0); /* echo server does not echo CLOSE */
}

/* ---- MAIN ---- */

int main(void) {
  printf("Running streaming echo server tests...\n");

  test_unfragmented_echo();
  test_fragmented_echo();
  test_binary_echo();
  test_close_no_echo();

  printf("All streaming echo server tests passed.\n");
  return 0;
}
