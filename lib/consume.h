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

#ifndef WEBSOCKET_CONSUME_H
#define WEBSOCKET_CONSUME_H

#include "websocket.h"
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Walk a TCP buffer that may contain multiple concatenated frames,
 * dispatching each to wsParseInputFrameWithContext.
 *
 * buf        : raw receive buffer (modified in-place for unmasking)
 * bufLen     : number of valid bytes in buf
 * ctx        : message-assembly context (caller owns)
 * on_message : called for each complete TEXT/BINARY message
 * on_control : called for each control frame (PING/PONG/CLOSE)
 * user       : user pointer passed through to callbacks
 *
 * Returns the number of bytes consumed.
 * If a partial frame is at the end, the caller should move the unconsumed
 * bytes to the front of the buffer and append the next recv() result.
 *
 * Returns 0 on unrecoverable protocol error.
 */
size_t wsConsumeBuffer(uint8_t *buf, size_t bufLen,
                       struct wsMessageContext *ctx,
                       ws_on_message_cb on_message, ws_on_control_cb on_control,
                       void *user);

#ifdef __cplusplus
}
#endif

#endif /* WEBSOCKET_CONSUME_H */
