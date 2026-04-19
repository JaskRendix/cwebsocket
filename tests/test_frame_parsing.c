#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "../lib/websocket.h"

/* Utility: build a masked client frame */
static size_t make_masked_frame(uint8_t opcode, const uint8_t *payload,
                                size_t payload_len, uint8_t *out) {
  uint8_t mask[4] = {0x11, 0x22, 0x33, 0x44};
  size_t offset = 0;

  out[offset++] = 0x80 | opcode;
  if (payload_len <= 125) {
    out[offset++] = 0x80 | (uint8_t)payload_len;
  } else if (payload_len <= 0xFFFF) {
    out[offset++] = 0x80 | 126;
    uint16_t len16 = htons((uint16_t)payload_len);
    memcpy(&out[offset], &len16, 2);
    offset += 2;
  } else {
    out[offset++] = 0x80 | 127;
    uint64_t len64 = htonll((uint64_t)payload_len);
    memcpy(&out[offset], &len64, 8);
    offset += 8;
  }

  memcpy(&out[offset], mask, 4);
  offset += 4;

  for (size_t i = 0; i < payload_len; i++)
    out[offset++] = payload[i] ^ mask[i % 4];

  return offset;
}

/* Test: simple masked text frame */
static void test_simple_text_frame(void) {
  uint8_t frame[64];
  const uint8_t payload[] = "Hi";
  size_t frame_len = make_masked_frame(WS_TEXT_FRAME, payload, 2, frame);

  uint8_t *dataPtr = NULL;
  size_t dataLen = 0;

  enum wsFrameType t = wsParseInputFrame(frame, frame_len, &dataPtr, &dataLen);
  assert(t == WS_TEXT_FRAME);
  assert(dataLen == 2);
  assert(memcmp(dataPtr, "Hi", 2) == 0);
}

/* Test: masked binary frame with 16-bit length */
static void test_16bit_length(void) {
  uint8_t payload[300];
  for (size_t i = 0; i < sizeof(payload); i++)
    payload[i] = (uint8_t)i;

  uint8_t frame[512];
  size_t frame_len =
      make_masked_frame(WS_BINARY_FRAME, payload, sizeof(payload), frame);

  uint8_t *dataPtr = NULL;
  size_t dataLen = 0;

  enum wsFrameType t = wsParseInputFrame(frame, frame_len, &dataPtr, &dataLen);
  assert(t == WS_BINARY_FRAME);
  assert(dataLen == sizeof(payload));
  assert(memcmp(dataPtr, payload, sizeof(payload)) == 0);
}

/* Test: missing MASK bit → error */
static void test_missing_mask(void) {
  uint8_t frame[] = {0x81, 0x02, 'O', 'K'};

  uint8_t *dataPtr = NULL;
  size_t dataLen = 0;

  assert(wsParseInputFrame(frame, sizeof(frame), &dataPtr, &dataLen) ==
         WS_ERROR_FRAME);
}

/* Test: incomplete frame */
static void test_incomplete_frame(void) {
  uint8_t frame[] = {0x81};

  uint8_t *dataPtr = NULL;
  size_t dataLen = 0;

  assert(wsParseInputFrame(frame, sizeof(frame), &dataPtr, &dataLen) ==
         WS_INCOMPLETE_FRAME);
}

/* Test: RSV bits set → error */
static void test_rsv_bits_set(void) {
  uint8_t frame[] = {0x90, 0x80 | 1, 0, 0, 0, 0, 0xAA};

  uint8_t *p = NULL;
  size_t n = 0;

  assert(wsParseInputFrame(frame, sizeof(frame), &p, &n) == WS_ERROR_FRAME);
}

/* Test: control FIN=0 → error */
static void test_control_fin_zero(void) {
  uint8_t frame[] = {0x09, 0x80 | 1, 0, 0, 0, 0, 'X'};

  uint8_t *p = NULL;
  size_t n = 0;

  assert(wsParseInputFrame(frame, sizeof(frame), &p, &n) == WS_ERROR_FRAME);
}

/* Test: control payload >125 → error */
static void test_control_payload_too_large(void) {
  uint8_t frame[300 + 10];
  frame[0] = 0x89;
  frame[1] = 0x80 | 126;

  uint16_t len = htons(200);
  memcpy(&frame[2], &len, 2);

  memset(&frame[4], 0, 4);
  memset(&frame[8], 'A', 200);

  uint8_t *p = NULL;
  size_t n = 0;

  assert(wsParseInputFrame(frame, sizeof(frame), &p, &n) == WS_ERROR_FRAME);
}

/* Test: reserved opcode → error */
static void test_reserved_opcode(void) {
  uint8_t frame[] = {0x83, 0x80 | 1, 0, 0, 0, 0, 0xAA};

  uint8_t *p = NULL;
  size_t n = 0;

  assert(wsParseInputFrame(frame, sizeof(frame), &p, &n) == WS_ERROR_FRAME);
}

/* Test: 64-bit length MSB set → error */
static void test_64bit_length_msb_set(void) {
  uint8_t frame[32];
  frame[0] = 0x82;
  frame[1] = 0x80 | 127;

  frame[2] = 0x80;
  memset(&frame[3], 0, 7);

  memset(&frame[10], 0, 4);
  frame[14] = 0xAA;

  uint8_t *p = NULL;
  size_t n = 0;

  assert(wsParseInputFrame(frame, sizeof(frame), &p, &n) == WS_ERROR_FRAME);
}

int main(void) {
  printf("Running frame parsing tests...\n");

  test_simple_text_frame();
  test_16bit_length();
  test_missing_mask();
  test_incomplete_frame();
  test_rsv_bits_set();
  test_control_fin_zero();
  test_control_payload_too_large();
  test_reserved_opcode();
  test_64bit_length_msb_set();

  printf("All frame parsing tests passed.\n");
  return 0;
}
