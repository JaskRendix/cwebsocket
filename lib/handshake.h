/*
 * Copyright (c) 2014 Putilov Andrey
 *
 * MIT License
 */

#ifndef WEBSOCKET_HANDSHAKE_H
#define WEBSOCKET_HANDSHAKE_H

#include "websocket.h"
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

enum wsFrameType wsParseHandshake(const uint8_t *inputFrame, size_t inputLength,
                                  struct handshake *hs);

void wsGetHandshakeAnswer(const struct handshake *hs, uint8_t *outFrame,
                          size_t *outLength);

void nullHandshake(struct handshake *hs);
void freeHandshake(struct handshake *hs);

#ifdef __cplusplus
}
#endif

#endif
