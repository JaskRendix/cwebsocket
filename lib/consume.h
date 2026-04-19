/*
 * Copyright (c) 2014 Putilov Andrey
 *
 * MIT License
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
intptr_t wsConsumeBuffer(uint8_t *buf, size_t bufLen,
                         struct wsMessageContext *ctx,
                         ws_on_message_cb on_message,
                         ws_on_control_cb on_control, void *user);

#ifdef __cplusplus
}
#endif

#endif /* WEBSOCKET_CONSUME_H */
