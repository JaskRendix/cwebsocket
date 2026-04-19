#include "websocket.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static int message_count = 0;
static char received_payload[256];

static void on_message(enum wsFrameType type, uint8_t *data, size_t len,
                       void *user) {
  (void)type;
  (void)user;

  message_count++;
  memcpy(received_payload, data, len);
  received_payload[len] = '\0';
}

static void test_fragmented_consumption(void) {
  printf("Testing fragmented consumption with continuation...\n");

  uint8_t msg_buffer[512];
  struct wsMessageContext ctx;
  wsInitMessageContext(&ctx, msg_buffer, sizeof(msg_buffer));

  /* Build first fragment normally (TEXT), then clear FIN to mark it fragmented
   */
  uint8_t f1[64];
  size_t f1_len = sizeof(f1);
  uint8_t mask1[4] = {0x11, 0x22, 0x33, 0x44};

  wsMakeClientFrame((const uint8_t *)"Hello ", 6, f1, &f1_len, WS_TEXT_FRAME,
                    mask1);

  /* FIN bit is set by wsMakeClientFrame; clear it to indicate "more fragments
   * follow" */
  f1[0] &= 0x7Fu; /* clear FIN, keep RSV/opcode */

  /* Build second fragment as TEXT, then patch opcode to CONT (0x00) */
  uint8_t f2[64];
  size_t f2_len = sizeof(f2);
  uint8_t mask2[4] = {0x55, 0x66, 0x77, 0x88};

  wsMakeClientFrame((const uint8_t *)"World", 5, f2, &f2_len, WS_TEXT_FRAME,
                    mask2);

  /* Keep FIN=1, clear opcode bits → CONTINUATION frame */
  f2[0] &= 0xF0u; /* keep FIN+RSV, opcode=0 */

  uint8_t stream[128];
  memcpy(stream, f1, f1_len);
  memcpy(stream + f1_len, f2, f2_len);

  size_t total_wire_size = f1_len + f2_len;

  /* Test 1: Full consumption */
  message_count = 0;

  intptr_t consumed =
      wsConsumeBuffer(stream, total_wire_size, &ctx, on_message, NULL, NULL);

  assert(consumed == (intptr_t)total_wire_size);
  assert(message_count == 1);
  assert(strcmp(received_payload, "Hello World") == 0);

  printf("  - Full stream consumption: PASS\n");

  /* Test 2: Partial header */
  wsResetMessageContext(&ctx);
  message_count = 0;

  consumed = wsConsumeBuffer(stream, 4, &ctx, on_message, NULL, NULL);

  assert(consumed == 0);
  assert(message_count == 0);

  printf("  - Partial header handling: PASS\n");

  /* Test 3: Invalid continuation */
  wsResetMessageContext(&ctx);

  uint8_t bad1[32], bad2[32];
  size_t bad1_len = sizeof(bad1);
  size_t bad2_len = sizeof(bad2);

  uint8_t m1[4] = {0x10, 0x20, 0x30, 0x40};
  uint8_t m2[4] = {0x50, 0x60, 0x70, 0x80};

  /* First fragment: TEXT, FIN=0 */
  wsMakeClientFrame((const uint8_t *)"A", 1, bad1, &bad1_len, WS_TEXT_FRAME,
                    m1);
  bad1[0] &= 0x7Fu; /* clear FIN */

  /* Second fragment: ILLEGAL TEXT instead of CONT (FIN=1, opcode=TEXT) */
  wsMakeClientFrame((const uint8_t *)"B", 1, bad2, &bad2_len, WS_TEXT_FRAME,
                    m2);
  /* leave opcode as TEXT → protocol violation */

  uint8_t invalid_stream[64];
  memcpy(invalid_stream, bad1, bad1_len);
  memcpy(invalid_stream + bad1_len, bad2, bad2_len);

  consumed = wsConsumeBuffer(invalid_stream, bad1_len + bad2_len, &ctx,
                             on_message, NULL, NULL);

  assert(consumed == -1);

  printf("  - Protocol violation (illegal opcode): PASS\n");
}

int main(void) {
  test_fragmented_consumption();
  printf("ALL CONSUME TESTS PASSED\n");
  return 0;
}
