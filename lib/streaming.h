/*
 * Copyright (c) 2014 Putilov Andrey
 *
 * MIT License
 */

#ifndef WEBSOCKET_STREAMING_H
#define WEBSOCKET_STREAMING_H

#include "websocket.h"
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Parse one WebSocket frame and dispatch to streaming callbacks.
 * For fragmented messages, on_begin fires once per fragment with that
 * fragment's payload length. Returns the frame type; WS_INCOMPLETE_FRAME
 * for non-final continuation frames.
 */
enum wsFrameType wsParseInputFrameStream(uint8_t *inputFrame,
                                         size_t inputLength,
                                         struct wsStreamCallbacks *cbs);

#ifdef __cplusplus
}
#endif

#endif /* WEBSOCKET_STREAMING_H */
