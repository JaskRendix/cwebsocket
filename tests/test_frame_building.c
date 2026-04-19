#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "../lib/websocket.h"

/* Utility: compare two byte arrays */
static void assert_bytes(const uint8_t *a, const uint8_t *b, size_t n) {
  for (size_t i = 0; i < n; i++) {
    if (a[i] != b[i]) {
      printf("Byte mismatch at %zu: got %02X expected %02X\n", i, a[i], b[i]);
      assert(0);
    }
  }
}

/* Test: small text frame (payload <= 125) */
static void test_small_text_frame(void) {
  const uint8_t payload[] = "Hi";
  uint8_t frame[64];
  size_t outLen = sizeof(frame);

  wsMakeFrame(payload, 2, frame, &outLen, WS_TEXT_FRAME);

  uint8_t expected[] = {0x81, 0x02, 'H', 'i'};
  assert(outLen == sizeof(expected));
  assert_bytes(frame, expected, sizeof(expected));
}

/* Test: 16-bit length frame (126) */
static void test_16bit_length_frame(void) {
  uint8_t payload[300];
  for (size_t i = 0; i < sizeof(payload); i++)
    payload[i] = (uint8_t)i;

  uint8_t frame[512];
  size_t outLen = sizeof(frame);

  wsMakeFrame(payload, sizeof(payload), frame, &outLen, WS_BINARY_FRAME);

  assert(frame[0] == 0x82);
  assert(frame[1] == 0x7E);

  uint16_t len16 = (uint16_t)((frame[2] << 8) | frame[3]);
  assert(len16 == sizeof(payload));
  assert(memcmp(&frame[4], payload, sizeof(payload)) == 0);
  assert(outLen == 4 + sizeof(payload));
}

/* Test: zero-length payload */
static void test_zero_length(void) {
  uint8_t frame[16];
  size_t outLen = sizeof(frame);

  wsMakeFrame(NULL, 0, frame, &outLen, WS_TEXT_FRAME);

  uint8_t expected[] = {0x81, 0x00};
  assert(outLen == sizeof(expected));
  assert_bytes(frame, expected, sizeof(expected));
}

/* Test: FIN + opcode correctness */
static void test_opcode_bits(void) {
  uint8_t payload[] = {0x11, 0x22};
  uint8_t frame[64];
  size_t outLen = sizeof(frame);

  wsMakeFrame(payload, 2, frame, &outLen, WS_PING_FRAME);

  assert(frame[0] == (0x80 | WS_PING_FRAME));
  assert(frame[1] == 2);
  assert(frame[2] == 0x11);
  assert(frame[3] == 0x22);
}

/* Test: CLOSE code only */
static void test_close_code_only(void) {
  uint8_t frame[32];
  size_t outLen = sizeof(frame);

  wsMakeCloseFrame(WS_CLOSE_NORMAL, NULL, frame, &outLen, NULL);

  assert(frame[0] == 0x88);
  assert(frame[1] == 0x02);
  assert(frame[2] == 0x03);
  assert(frame[3] == 0xE8);
}

/* Test: masked CLOSE frame */
static void test_close_masked(void) {
  uint8_t frame[64];
  size_t outLen = sizeof(frame);
  uint8_t mask[4] = {1, 2, 3, 4};

  wsMakeCloseFrame(WS_CLOSE_NORMAL, "X", frame, &outLen, mask);

  assert(frame[1] & 0x80);
}

int main(void) {
  printf("Running frame building tests...\n");

  test_small_text_frame();
  test_16bit_length_frame();
  test_zero_length();
  test_opcode_bits();
  test_close_code_only();
  test_close_masked();

  printf("All frame building tests passed.\n");
  return 0;
}
