/*
 * Copyright (c) 2014 Putilov Andrey
 *
 * MIT License
 */

#ifndef WEBSOCKET_FRAME_BUILDER_H
#define WEBSOCKET_FRAME_BUILDER_H

#include "websocket.h"
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Build a server-side (unmasked) WebSocket frame from payload */
void wsMakeFrame(const uint8_t *data, size_t dataLength, uint8_t *outFrame,
                 size_t *outLength, enum wsFrameType frameType);

/*
 * Build a client-side (masked) WebSocket frame from payload.
 * mask must point to 4 bytes of masking key (caller provides entropy).
 * outFrame must be at least dataLength + 14 bytes.
 */
void wsMakeClientFrame(const uint8_t *data, size_t dataLength,
                       uint8_t *outFrame, size_t *outLength,
                       enum wsFrameType frameType, const uint8_t mask[4]);

/*
 * Build a CLOSE frame with a status code and optional UTF-8 reason.
 * reason may be NULL or empty. outFrame must be at least 2+len(reason)+14.
 * For client-side use, pass a non-NULL mask; for server-side pass NULL.
 */
void wsMakeCloseFrame(enum wsCloseCode code, const char *reason,
                      uint8_t *outFrame, size_t *outLength,
                      const uint8_t mask[4]);

#ifdef __cplusplus
}
#endif

#endif /* WEBSOCKET_FRAME_BUILDER_H */
