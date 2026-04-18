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

#ifndef WEBSOCKET_H
#define WEBSOCKET_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stddef.h>

#include <netinet/in.h>  /* htons, ntohs */
#include "aw-base64.h"
#include "aw-sha1.h"

/* 64‑bit host/network byte order helpers */
static inline uint64_t htonll(uint64_t x) {
#if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
    return ((uint64_t)htonl((uint32_t)(x >> 32)) |
           ((uint64_t)htonl((uint32_t)(x & 0xffffffffu)) << 32));
#else
    return x;
#endif
}

static inline uint64_t ntohll(uint64_t x) {
    return htonll(x);
}

/* Boolean-style flags kept for compatibility */
#ifndef TRUE
# define TRUE 1
#endif
#ifndef FALSE
# define FALSE 0
#endif

/* HTTP/WebSocket header constants */
static const char connectionField[] = "Connection: ";
static const char upgrade[]         = "upgrade";
static const char upgrade2[]        = "Upgrade";
static const char upgradeField[]    = "Upgrade: ";
static const char websocket[]       = "websocket";
static const char hostField[]       = "Host: ";
static const char originField[]     = "Origin: ";
static const char keyField[]        = "Sec-WebSocket-Key: ";
static const char protocolField[]   = "Sec-WebSocket-Protocol: ";
static const char versionField[]    = "Sec-WebSocket-Version: ";
static const char version[]         = "13";
static const char secret[]          = "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";

enum wsFrameType { /* errors starting from 0xF0 */
    WS_EMPTY_FRAME      = 0xF0,
    WS_ERROR_FRAME      = 0xF1,
    WS_INCOMPLETE_FRAME = 0xF2,
    WS_TEXT_FRAME       = 0x01,
    WS_BINARY_FRAME     = 0x02,
    WS_PING_FRAME       = 0x09,
    WS_PONG_FRAME       = 0x0A,
    WS_OPENING_FRAME    = 0xF3,
    WS_CLOSING_FRAME    = 0x08
};

enum wsState {
    WS_STATE_OPENING,
    WS_STATE_NORMAL,
    WS_STATE_CLOSING
};

struct handshake {
    char *host;
    char *origin;
    char *key;
    char *resource;
    enum wsFrameType frameType;
};

/* Parse HTTP Upgrade request into handshake */
enum wsFrameType
wsParseHandshake(const uint8_t *inputFrame,
                 size_t inputLength,
                 struct handshake *hs);

/* Build HTTP 101 Switching Protocols response */
void
wsGetHandshakeAnswer(const struct handshake *hs,
                     uint8_t *outFrame,
                     size_t *outLength);

/* Build a WebSocket frame from payload */
void
wsMakeFrame(const uint8_t *data,
            size_t dataLength,
            uint8_t *outFrame,
            size_t *outLength,
            enum wsFrameType frameType);

/* Parse a WebSocket frame in-place, unmasking payload */
enum wsFrameType
wsParseInputFrame(uint8_t *inputFrame,
                  size_t inputLength,
                  uint8_t **dataPtr,
                  size_t *dataLength);

/* Initialize handshake struct to NULL/empty */
void
nullHandshake(struct handshake *hs);

/* Free all handshake-owned memory and reset */
void
freeHandshake(struct handshake *hs);

/* -------- Continuation-frame message assembly -------- */

struct wsMessageContext {
    uint8_t *buffer;
    size_t   capacity;
    size_t   length;
    uint8_t  opcode;       /* WS_TEXT_FRAME or WS_BINARY_FRAME */
    int      in_progress;  /* 0 = no fragmented message, 1 = assembling */
};

void
wsInitMessageContext(struct wsMessageContext *ctx,
                     uint8_t *buffer,
                     size_t capacity);

void
wsResetMessageContext(struct wsMessageContext *ctx);

enum wsFrameType
wsParseInputFrameWithContext(uint8_t *inputFrame,
                             size_t inputLength,
                             uint8_t **dataPtr,
                             size_t *dataLength,
                             struct wsMessageContext *ctx);

/* -------- Streaming callbacks (no buffering) -------- */

typedef void (*ws_on_message_begin_cb)(uint8_t opcode,
                                       size_t total_length,
                                       void *user);

typedef void (*ws_on_message_data_cb)(const uint8_t *data,
                                      size_t len,
                                      void *user);

typedef void (*ws_on_message_end_cb)(void *user);

struct wsStreamCallbacks {
    ws_on_message_begin_cb on_begin;
    ws_on_message_data_cb  on_data;
    ws_on_message_end_cb   on_end;
    void *user;
    /* internal state — zero-initialise, do not touch */
    uint8_t _opcode;
    int     _in_progress;
};

enum wsFrameType
wsParseInputFrameStream(uint8_t *inputFrame,
                        size_t inputLength,
                        struct wsStreamCallbacks *cbs);

#ifdef __cplusplus
}
#endif

#endif /* WEBSOCKET_H */
