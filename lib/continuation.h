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
