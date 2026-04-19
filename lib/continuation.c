/*
 * Copyright (c) 2014 Putilov Andrey
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#include "continuation.h"
#include "frame_parser.h"

#include <assert.h>
#include <string.h>

void wsInitMessageContext(struct wsMessageContext *ctx, uint8_t *buffer,
                          size_t capacity) {
  ctx->buffer = buffer;
  ctx->capacity = capacity;
  ctx->length = 0;
  ctx->opcode = 0;
  ctx->in_progress = 0;
}

void wsResetMessageContext(struct wsMessageContext *ctx) {
  ctx->length = 0;
  ctx->opcode = 0;
  ctx->in_progress = 0;
}

enum wsFrameType wsParseInputFrameWithContext(uint8_t *inputFrame,
                                              size_t inputLength,
                                              uint8_t **dataPtr,
                                              size_t *dataLength,
                                              struct wsMessageContext *ctx) {
  uint8_t *payload = NULL;
  size_t plen = 0;

  enum wsFrameType t =
      wsParseInputFrameSingle(inputFrame, inputLength, &payload, &plen);

  /* Control frames and errors pass through unchanged */
  if (t == WS_ERROR_FRAME || t == WS_EMPTY_FRAME || t == WS_PING_FRAME ||
      t == WS_PONG_FRAME || t == WS_CLOSING_FRAME || t == WS_OPENING_FRAME) {
    *dataPtr = payload;
    *dataLength = plen;
    return t;
  }

  uint8_t fin = (inputFrame[0] & 0x80u) != 0;
  uint8_t opcode = (inputFrame[0] & 0x0Fu);

  /* ---- Unfragmented TEXT/BINARY ---- */
  if ((opcode == WS_TEXT_FRAME || opcode == WS_BINARY_FRAME) && fin == 1 &&
      ctx->in_progress == 0) {
    *dataPtr = payload;
    *dataLength = plen;
    return t;
  }

  /* ---- Start of fragmented message ---- */
  if ((opcode == WS_TEXT_FRAME || opcode == WS_BINARY_FRAME) && fin == 0 &&
      ctx->in_progress == 0) {
    if (plen > ctx->capacity)
      return WS_ERROR_FRAME;

    memcpy(ctx->buffer, payload, plen);
    ctx->length = plen;
    ctx->opcode = opcode;
    ctx->in_progress = 1;

    return WS_INCOMPLETE_FRAME;
  }

  /* ---- Illegal continuation start ---- */
  if (opcode == 0x00 && ctx->in_progress == 0)
    return WS_ERROR_FRAME;

  /* ---- Illegal new data frame during continuation ---- */
  if ((opcode == WS_TEXT_FRAME || opcode == WS_BINARY_FRAME) &&
      ctx->in_progress == 1)
    return WS_ERROR_FRAME;

  /* ---- Middle continuation ---- */
  if (opcode == 0x00 && fin == 0 && ctx->in_progress == 1) {
    if (ctx->length + plen > ctx->capacity)
      return WS_ERROR_FRAME;

    memcpy(ctx->buffer + ctx->length, payload, plen);
    ctx->length += plen;

    return WS_INCOMPLETE_FRAME;
  }

  /* ---- Final continuation ---- */
  if (opcode == 0x00 && fin == 1 && ctx->in_progress == 1) {
    if (ctx->length + plen > ctx->capacity)
      return WS_ERROR_FRAME;

    memcpy(ctx->buffer + ctx->length, payload, plen);
    ctx->length += plen;

    *dataPtr = ctx->buffer;
    *dataLength = ctx->length;

    enum wsFrameType final_type =
        (ctx->opcode == WS_TEXT_FRAME) ? WS_TEXT_FRAME : WS_BINARY_FRAME;

    wsResetMessageContext(ctx);
    return final_type;
  }

  return WS_ERROR_FRAME;
}
