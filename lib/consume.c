/*
 * Copyright (c) 2014 Putilov Andrey
 *
 * MIT License
 */

#include "consume.h"
#include "continuation.h"
#include "frame_parser.h"

#include <assert.h>
#include <string.h>

size_t wsConsumeBuffer(uint8_t *buf, size_t bufLen,
                       struct wsMessageContext *ctx,
                       ws_on_message_cb on_message, ws_on_control_cb on_control,
                       void *user) {
  assert(buf);
  assert(ctx);

  size_t consumed = 0;

  while (consumed < bufLen) {
    uint8_t *frame = buf + consumed;
    size_t remaining = bufLen - consumed;

    /* Need at least 2 bytes to determine frame size */
    if (remaining < 2)
      break;

    /* Determine payload length from header */
    uint8_t len7 = (uint8_t)(frame[1] & 0x7Fu);
    size_t header_len;
    size_t payload_len;

    if (len7 == 0x7Eu) {
      if (remaining < 4)
        break; /* incomplete header */
      uint16_t v16 = 0;
      memcpy(&v16, &frame[2], 2);
      payload_len = (size_t)ntohs(v16);
      header_len = 4;
    } else if (len7 == 0x7Fu) {
      if (remaining < 10)
        break; /* incomplete header */
      uint64_t v64 = 0;
      memcpy(&v64, &frame[2], 8);
      payload_len = (size_t)ntohll(v64);
      header_len = 10;
    } else {
      payload_len = (size_t)len7;
      header_len = 2;
    }

    /* Account for masking key if present */
    int masked = (frame[1] & 0x80u) != 0;
    if (masked)
      header_len += 4;

    size_t frame_len = header_len + payload_len;

    /* Partial frame at end of buffer */
    if (remaining < frame_len)
      break;

    /* Parse the frame */
    uint8_t *dataPtr = NULL;
    size_t dataLength = 0;

    enum wsFrameType t = wsParseInputFrameWithContext(
        frame, frame_len, &dataPtr, &dataLength, ctx);

    if (t == WS_ERROR_FRAME) {
      /* Unrecoverable protocol error */
      return 0;
    }

    /* Dispatch control frames */
    if (t == WS_PING_FRAME || t == WS_PONG_FRAME || t == WS_CLOSING_FRAME) {
      if (on_control)
        on_control(t, dataPtr, dataLength, user);
    }
    /* Dispatch complete TEXT/BINARY messages */
    else if (t == WS_TEXT_FRAME || t == WS_BINARY_FRAME) {
      if (on_message)
        on_message(t, dataPtr, dataLength, user);
    }
    /* WS_INCOMPLETE_FRAME = continuation in progress */

    consumed += frame_len;
  }

  return consumed;
}
