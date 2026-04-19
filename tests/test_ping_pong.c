#include "websocket.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static void test_make_pong(void) {
  uint8_t ping_payload[] = "hello";
  uint8_t out[WS_MAX_HEADER_SIZE + 5];
  size_t out_len = sizeof(out);

  wsMakePongFrame(ping_payload, 5, out, &out_len);

  /* FIN=1 (0x80) | PONG opcode (0x0A) = 0x8A */
  assert(out[0] == 0x8A);
  assert(out[1] == 5);
  assert(memcmp(&out[2], ping_payload, 5) == 0);

  printf("Test Make Pong: PASSED\n");
}

static void test_invalid_ping_length(void) {
  struct wsMessageContext ctx;
  uint8_t msg_buf[128];
  wsInitMessageContext(&ctx, msg_buf, sizeof(msg_buf));

  /* PING with payload length 126 → illegal for control frames */
  uint8_t frame[] = {0x89, 126, 0, 0};
  uint8_t *data;
  size_t data_len;

  enum wsFrameType t = wsParseInputFrameWithContext(frame, sizeof(frame), &data,
                                                    &data_len, &ctx);

  assert(t == WS_ERROR_FRAME);
  printf("Test Invalid Ping Length: PASSED\n");
}

static void test_fragmented_ping_rejection(void) {
  struct wsMessageContext ctx;
  uint8_t msg_buf[128];
  wsInitMessageContext(&ctx, msg_buf, sizeof(msg_buf));

  /* PING with FIN=0 → illegal fragmented control frame */
  uint8_t frame[] = {0x09, 0};
  uint8_t *data;
  size_t data_len;

  enum wsFrameType t = wsParseInputFrameWithContext(frame, sizeof(frame), &data,
                                                    &data_len, &ctx);

  assert(t == WS_ERROR_FRAME);
  printf("Test Fragmented Ping Rejection: PASSED\n");
}

int main(void) {
  test_make_pong();
  test_invalid_ping_length();
  test_fragmented_ping_rejection();
  return 0;
}
