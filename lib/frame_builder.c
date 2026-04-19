/*
 * Copyright (c) 2014 Putilov Andrey
 *
 * MIT License
 */

#include "frame_builder.h"

#include <assert.h>
#include <string.h>

/* ---- internal frame header writer (shared by Make and MakeClient) ---- */
static size_t writeFrameHeader(uint8_t *outFrame, uint8_t firstByte,
                               size_t dataLength, int masked) {
  size_t offset = 0;
  outFrame[offset++] = firstByte;

  uint8_t mask_bit = masked ? 0x80u : 0x00u;

  if (dataLength <= 125u) {
    outFrame[offset++] = mask_bit | (uint8_t)dataLength;
  } else if (dataLength <= 0xFFFFu) {
    outFrame[offset++] = mask_bit | 126u;
    uint16_t len16 = htons((uint16_t)dataLength);
    memcpy(&outFrame[offset], &len16, sizeof(len16));
    offset += sizeof(len16);
  } else {
    outFrame[offset++] = mask_bit | 127u;
    uint64_t len64 = htonll((uint64_t)dataLength);
    memcpy(&outFrame[offset], &len64, sizeof(len64));
    offset += sizeof(len64);
  }

  return offset;
}

static void wsValidateControlFrame(enum wsFrameType frameType,
                                   size_t dataLength) {
  if (frameType == WS_PING_FRAME || frameType == WS_PONG_FRAME ||
      frameType == WS_CLOSING_FRAME) {
    /* RFC 6455: control frames must not be fragmented and must have <=125 bytes
     */
    assert(dataLength <= 125u);
  }
}

void wsMakeFrame(const uint8_t *data, size_t dataLength, uint8_t *outFrame,
                 size_t *outLength, enum wsFrameType frameType) {
  assert(outFrame);
  assert(outLength);
  assert(frameType < 0x10);
  if (dataLength > 0)
    assert(data);

  wsValidateControlFrame(frameType, dataLength);

  size_t header_len = 2;
  if (dataLength > 125u && dataLength <= 0xFFFFu)
    header_len += 2;
  else if (dataLength > 0xFFFFu)
    header_len += 8;

  assert(*outLength >= header_len + dataLength);

  uint8_t first = (uint8_t)(0x80u | (uint8_t)frameType);
  size_t offset = writeFrameHeader(outFrame, first, dataLength, 0);

  if (dataLength > 0) {
    memcpy(&outFrame[offset], data, dataLength);
    offset += dataLength;
  }

  *outLength = offset;
}

void wsMakeClientFrame(const uint8_t *data, size_t dataLength,
                       uint8_t *outFrame, size_t *outLength,
                       enum wsFrameType frameType, const uint8_t mask[4]) {
  assert(outFrame);
  assert(outLength);
  assert(frameType < 0x10);
  assert(mask);
  if (dataLength > 0)
    assert(data);

  wsValidateControlFrame(frameType, dataLength);

  size_t header_len = 2 + 4; /* base + mask */
  if (dataLength > 125u && dataLength <= 0xFFFFu)
    header_len += 2;
  else if (dataLength > 0xFFFFu)
    header_len += 8;

  assert(*outLength >= header_len + dataLength);

  uint8_t first = (uint8_t)(0x80u | (uint8_t)frameType);
  size_t offset = writeFrameHeader(outFrame, first, dataLength, 1);

  memcpy(&outFrame[offset], mask, 4);
  offset += 4;

  for (size_t i = 0; i < dataLength; ++i)
    outFrame[offset++] = data[i] ^ mask[i % 4u];

  *outLength = offset;
}

void wsMakeCloseFrame(enum wsCloseCode code, const char *reason,
                      uint8_t *outFrame, size_t *outLength,
                      const uint8_t mask[4]) {
  assert(outFrame);
  assert(outLength);

  size_t reason_len = reason ? strlen(reason) : 0;

  /* RFC 6455: control payload <=125, close code uses 2 bytes, reason <=123 */
  if (code != WS_CLOSE_NO_STATUS && code != WS_CLOSE_ABNORMAL) {
    assert(reason_len <= 123u);
  }

  size_t payload_len = (code != WS_CLOSE_NO_STATUS && code != WS_CLOSE_ABNORMAL)
                           ? 2 + reason_len
                           : 0;

  uint8_t payload[125];
  assert(payload_len <= sizeof(payload));

  if (payload_len >= 2) {
    payload[0] = (uint8_t)((unsigned int)code >> 8);
    payload[1] = (uint8_t)((unsigned int)code & 0xFFu);
    if (reason_len > 0)
      memcpy(&payload[2], reason, reason_len);
  }

  if (mask) {
    wsMakeClientFrame(payload_len > 0 ? payload : NULL, payload_len, outFrame,
                      outLength, WS_CLOSING_FRAME, mask);
  } else {
    wsMakeFrame(payload_len > 0 ? payload : NULL, payload_len, outFrame,
                outLength, WS_CLOSING_FRAME);
  }
}
