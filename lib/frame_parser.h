/*
 * Copyright (c) 2014 Putilov Andrey
 *
 * MIT License
 */

#ifndef WEBSOCKET_FRAME_PARSER_H
#define WEBSOCKET_FRAME_PARSER_H

#include "websocket.h"
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Parse a single WebSocket frame in-place, unmasking payload.
 * Returns:
 *   WS_TEXT_FRAME / WS_BINARY_FRAME / WS_PING_FRAME / WS_PONG_FRAME /
 *   WS_CLOSING_FRAME — valid frames
 *   WS_INCOMPLETE_FRAME — not enough bytes
 *   WS_ERROR_FRAME — protocol violation
 */
enum wsFrameType wsParseInputFrameSingle(uint8_t *inputFrame,
                                         size_t inputLength, uint8_t **dataPtr,
                                         size_t *dataLength);

/*
 * Stateless wrapper around wsParseInputFrameWithContext.
 * Suitable for single-frame parsing without continuation support.
 */
enum wsFrameType wsParseInputFrame(uint8_t *inputFrame, size_t inputLength,
                                   uint8_t **dataPtr, size_t *dataLength);

#ifdef __cplusplus
}
#endif

#endif /* WEBSOCKET_FRAME_PARSER_H */
