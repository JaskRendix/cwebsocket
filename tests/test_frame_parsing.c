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

  out[offset++] = 0x80 | opcode; /* FIN=1, opcode */
  if (payload_len <= 125) {
    out[offset++] = 0x80 | (uint8_t)payload_len; /* MASK=1 */
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

  for (size_t i = 0; i < payload_len; i++) {
    out[offset++] = payload[i] ^ mask[i % 4];
  }

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

/* Test: masked binary frame with 64-bit length */
static void test_64bit_length(void) {
  /* Small payload but force 64-bit length */
  uint8_t payload[1] = {0xAB};

  uint8_t frame[64];
  uint8_t mask[4] = {0x11, 0x22, 0x33, 0x44};
  size_t offset = 0;

  frame[offset++] = 0x82;       /* FIN=1, BINARY */
  frame[offset++] = 0x80 | 127; /* MASK=1, 64-bit length */

  uint64_t len64 = htonll(1);
  memcpy(&frame[offset], &len64, 8);
  offset += 8;

  memcpy(&frame[offset], mask, 4);
  offset += 4;

  frame[offset++] = payload[0] ^ mask[0];

  uint8_t *dataPtr = NULL;
  size_t dataLen = 0;

  enum wsFrameType t = wsParseInputFrame(frame, offset, &dataPtr, &dataLen);
  assert(t == WS_BINARY_FRAME);
  assert(dataLen == 1);
  assert(dataPtr[0] == 0xAB);
}

/* Test: missing MASK bit → error */
static void test_missing_mask(void) {
  uint8_t frame[] = {0x81, /* FIN=1, TEXT */
                     0x02, /* MASK=0, length=2 → invalid for client frame */
                     'O', 'K'};

  uint8_t *dataPtr = NULL;
  size_t dataLen = 0;

  enum wsFrameType t =
      wsParseInputFrame(frame, sizeof(frame), &dataPtr, &dataLen);
  assert(t == WS_ERROR_FRAME);
}

/* Test: incomplete frame */
static void test_incomplete_frame(void) {
  uint8_t frame[] = {0x81}; /* only 1 byte */

  uint8_t *dataPtr = NULL;
  size_t dataLen = 0;

  enum wsFrameType t =
      wsParseInputFrame(frame, sizeof(frame), &dataPtr, &dataLen);
  assert(t == WS_INCOMPLETE_FRAME);
}

int main(void) {
  printf("Running frame parsing tests...\n");

  test_simple_text_frame();
  test_16bit_length();
  test_64bit_length();
  test_missing_mask();
  test_incomplete_frame();

  printf("All frame parsing tests passed.\n");
  return 0;
}
