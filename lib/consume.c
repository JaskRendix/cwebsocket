/*
 * Copyright (c) 2014 Putilov Andrey
 *
 * MIT License
 */

#include "consume.h"
#include "continuation.h"
#include "frame_parser.h"
#include "websocket.h"

#include <assert.h>
#include <limits.h>
#include <string.h>

intptr_t wsConsumeBuffer(uint8_t *buf, size_t bufLen,
                         struct wsMessageContext *ctx,
                         ws_on_message_cb on_message,
                         ws_on_control_cb on_control, void *user) {
  assert(buf);
  assert(ctx);

  size_t consumed = 0;

  while (consumed < bufLen) {
    uint8_t *frame = buf + consumed;
    size_t remaining = bufLen - consumed;

    if (remaining < 2)
      break;

    uint8_t len7 = (uint8_t)(frame[1] & 0x7Fu);
    size_t header_len = 2;
    size_t payload_len = 0;

    /* 1. Determine payload length */
    if (len7 == 0x7Eu) {
      if (remaining < 4)
        break;
      uint16_t v16;
      memcpy(&v16, &frame[2], 2);
      payload_len = (size_t)ntohs(v16);
      header_len = 4;

    } else if (len7 == 0x7Fu) {
      if (remaining < 10)
        break;
      uint64_t v64;
      memcpy(&v64, &frame[2], 8);
      v64 = ntohll(v64);

      if (v64 > 0x7FFFFFFFFFFFFFFFULL || v64 > (uint64_t)SIZE_MAX)
        return -1;

      payload_len = (size_t)v64;
      header_len = 10;

    } else {
      payload_len = (size_t)len7;
    }

    /* 1.5 RFC 6455 Section 5.5: Control Frame Validation */
    uint8_t opcode = frame[0] & 0x0Fu;
    if (opcode >= 0x08) { /* Control frame */
      int fin = (frame[0] & 0x80u);
      if (payload_len > 125 || !fin) {
        return -1; /* Protocol error */
      }
    }

    /* 2. Mask bit */
    if (frame[1] & 0x80u)
      header_len += 4;

    /* 3. Check completeness */
    size_t frame_total_len = header_len + payload_len;
    if (remaining < frame_total_len)
      break;

    /* 4. Parse and dispatch */
    uint8_t *dataPtr = NULL;
    size_t dataLength = 0;

    enum wsFrameType t = wsParseInputFrameWithContext(
        frame, frame_total_len, &dataPtr, &dataLength, ctx);

    if (t == WS_ERROR_FRAME)
      return -1;

    if (t == WS_PING_FRAME || t == WS_PONG_FRAME || t == WS_CLOSING_FRAME) {
      if (on_control)
        on_control(t, dataPtr, dataLength, user);

    } else if (t == WS_TEXT_FRAME || t == WS_BINARY_FRAME) {
      if (on_message)
        on_message(t, dataPtr, dataLength, user);
    }

    consumed += frame_total_len;
  }

  return (intptr_t)consumed;
}
