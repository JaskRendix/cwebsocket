/*
 * Copyright (c) 2014 Putilov Andrey
 *
 * MIT License
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

/* Control frames MUST NOT be fragmented and MUST have <=125 bytes */
static int ws_invalid_control_frame(uint8_t fin, size_t plen) {
  if (!fin)
    return 1;
  if (plen > 125)
    return 1;
  return 0;
}

/* Continuation MUST NOT appear unless a fragmented message is active */
static int ws_invalid_continuation_start(struct wsMessageContext *ctx) {
  return (ctx->in_progress == 0);
}

/* New TEXT/BINARY MUST NOT appear while continuation is active */
static int ws_invalid_new_data_frame(struct wsMessageContext *ctx) {
  return (ctx->in_progress != 0);
}

enum wsFrameType wsParseInputFrameWithContext(uint8_t *inputFrame,
                                              size_t inputLength,
                                              uint8_t **dataPtr,
                                              size_t *dataLength,
                                              struct wsMessageContext *ctx) {
  assert(ctx != NULL);

  uint8_t *payload = NULL;
  size_t plen = 0;

  enum wsFrameType t =
      wsParseInputFrameSingle(inputFrame, inputLength, &payload, &plen);

  /* Pass through errors and control frames unchanged */
  if (t == WS_ERROR_FRAME || t == WS_EMPTY_FRAME || t == WS_OPENING_FRAME ||
      t == WS_PING_FRAME || t == WS_PONG_FRAME || t == WS_CLOSING_FRAME) {
    /* Validate control frame rules */
    if (t == WS_PING_FRAME || t == WS_PONG_FRAME || t == WS_CLOSING_FRAME) {
      uint8_t fin = (inputFrame[0] & 0x80u) != 0;
      if (ws_invalid_control_frame(fin, plen))
        return WS_ERROR_FRAME;
    }

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

  /* ---- Illegal new data frame during continuation ---- */
  if ((opcode == WS_TEXT_FRAME || opcode == WS_BINARY_FRAME) &&
      ws_invalid_new_data_frame(ctx)) {
    return WS_ERROR_FRAME;
  }

  /* ---- Continuation frame with no active message ---- */
  if (opcode == 0x00 && ws_invalid_continuation_start(ctx))
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
