/*
 * Copyright (c) 2014 Putilov Andrey
 *
 * MIT License
 */

#include "frame_parser.h"

#include <assert.h>
#include <string.h>

/* ---- internal payload-length decoder ---- */
static size_t getPayloadLength(const uint8_t *inputFrame, size_t inputLength,
                               uint8_t *payloadFieldExtraBytes,
                               enum wsFrameType *frameType) {
  if (inputLength < 2) {
    *frameType = WS_INCOMPLETE_FRAME;
    return 0;
  }

  uint8_t len7 = (uint8_t)(inputFrame[1] & 0x7Fu);
  *payloadFieldExtraBytes = 0;

  /* Need enough bytes for extended length fields */
  if ((len7 == 0x7Eu && inputLength < 4) ||
      (len7 == 0x7Fu && inputLength < 10)) {
    *frameType = WS_INCOMPLETE_FRAME;
    return 0;
  }

  /* RFC 6455: 64-bit lengths must not have MSB set */
  if (len7 == 0x7Fu && (inputFrame[2] & 0x80u) != 0x00u) {
    *frameType = WS_ERROR_FRAME;
    return 0;
  }

  size_t payloadLength = 0;

  if (len7 == 0x7Eu) {
    uint16_t v16 = 0;
    *payloadFieldExtraBytes = 2;
    memcpy(&v16, &inputFrame[2], 2);
    payloadLength = (size_t)ntohs(v16);
  } else if (len7 == 0x7Fu) {
    uint64_t v64 = 0;
    *payloadFieldExtraBytes = 8;
    memcpy(&v64, &inputFrame[2], 8);
    v64 = ntohll(v64);

    if (v64 > (uint64_t)SIZE_MAX) {
      *frameType = WS_ERROR_FRAME;
      return 0;
    }

    payloadLength = (size_t)v64;
  } else {
    payloadLength = (size_t)len7;
  }

  return payloadLength;
}

/* ---- core single-frame parser ---- */
enum wsFrameType wsParseInputFrameSingle(uint8_t *inputFrame,
                                         size_t inputLength, uint8_t **dataPtr,
                                         size_t *dataLength) {
  assert(inputFrame);
  assert(dataPtr);
  assert(dataLength);

  if (inputLength < 2)
    return WS_INCOMPLETE_FRAME;

  /* RSV bits must be zero */
  if ((inputFrame[0] & 0x70u) != 0x00u)
    return WS_ERROR_FRAME;

  uint8_t opcode = (uint8_t)(inputFrame[0] & 0x0Fu);

  /* Client frames MUST be masked */
  if ((inputFrame[1] & 0x80u) != 0x80u)
    return WS_ERROR_FRAME;

  /* Valid opcodes */
  if (opcode != WS_TEXT_FRAME && opcode != WS_BINARY_FRAME &&
      opcode != WS_CLOSING_FRAME && opcode != WS_PING_FRAME &&
      opcode != WS_PONG_FRAME && opcode != 0x00) {
    return WS_ERROR_FRAME;
  }

  enum wsFrameType frameType = (enum wsFrameType)opcode;

  uint8_t extra = 0;
  size_t payloadLength =
      getPayloadLength(inputFrame, inputLength, &extra, &frameType);

  if (frameType == WS_INCOMPLETE_FRAME || frameType == WS_ERROR_FRAME)
    return frameType;

  size_t header_and_mask = 2u + (size_t)extra + 4u;

  if (header_and_mask + payloadLength > inputLength)
    return WS_INCOMPLETE_FRAME;

  uint8_t *maskingKey = &inputFrame[2 + extra];
  *dataPtr = &inputFrame[2 + extra + 4];
  *dataLength = payloadLength;

  /* Unmask payload in-place */
  for (size_t i = 0; i < *dataLength; ++i)
    (*dataPtr)[i] ^= maskingKey[i % 4u];

  return frameType;
}

/* ---- stateless wrapper ---- */
enum wsFrameType wsParseInputFrame(uint8_t *inputFrame, size_t inputLength,
                                   uint8_t **dataPtr, size_t *dataLength) {
  static uint8_t dummy[1];
  struct wsMessageContext ctx;

  /* Dummy context: no continuation support */
  wsInitMessageContext(&ctx, dummy, sizeof(dummy));

  return wsParseInputFrameWithContext(inputFrame, inputLength, dataPtr,
                                      dataLength, &ctx);
}
