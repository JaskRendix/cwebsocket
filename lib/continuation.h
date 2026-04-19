/*
 * Copyright (c) 2014 Putilov Andrey
 *
 * MIT License
 */

#ifndef WEBSOCKET_CONTINUATION_H
#define WEBSOCKET_CONTINUATION_H

#include "websocket.h"
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Initialize message-assembly context */
void wsInitMessageContext(struct wsMessageContext *ctx, uint8_t *buffer,
                          size_t capacity);

/* Reset context after a completed or aborted message */
void wsResetMessageContext(struct wsMessageContext *ctx);

/*
 * Parse a WebSocket frame with continuation support.
 * Returns:
 *   WS_TEXT_FRAME / WS_BINARY_FRAME — complete message
 *   WS_INCOMPLETE_FRAME            — fragment in progress
 *   WS_PING_FRAME / WS_PONG_FRAME / WS_CLOSING_FRAME — control frames
 *   WS_ERROR_FRAME                 — protocol violation
 */
enum wsFrameType wsParseInputFrameWithContext(uint8_t *inputFrame,
                                              size_t inputLength,
                                              uint8_t **dataPtr,
                                              size_t *dataLength,
                                              struct wsMessageContext *ctx);

#ifdef __cplusplus
}
#endif

#endif /* WEBSOCKET_CONTINUATION_H */
