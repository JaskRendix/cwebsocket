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

#include <stddef.h>
#include <stdint.h>

#include "aw-base64.h"
#include "aw-sha1.h"
#include <netinet/in.h> /* htons, ntohs */

/* 64-bit host/network byte order helpers */
static inline uint64_t htonll(uint64_t x) {
#if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
  return ((uint64_t)htonl((uint32_t)(x >> 32)) |
          ((uint64_t)htonl((uint32_t)(x & 0xffffffffu)) << 32));
#else
  return x;
#endif
}

static inline uint64_t ntohll(uint64_t x) { return htonll(x); }

/* Boolean-style flags kept for compatibility */
#ifndef TRUE
#define TRUE 1
#endif
#ifndef FALSE
#define FALSE 0
#endif

/* WebSocket frame types (RFC 6455 opcodes + internal markers) */
enum wsFrameType { /* errors starting from 0xF0 */
                   WS_EMPTY_FRAME = 0xF0,
                   WS_ERROR_FRAME = 0xF1,
                   WS_INCOMPLETE_FRAME = 0xF2,
                   WS_TEXT_FRAME = 0x01,
                   WS_BINARY_FRAME = 0x02,
                   WS_PING_FRAME = 0x09,
                   WS_PONG_FRAME = 0x0A,
                   WS_OPENING_FRAME = 0xF3,
                   WS_CLOSING_FRAME = 0x08
};

enum wsState { WS_STATE_OPENING, WS_STATE_NORMAL, WS_STATE_CLOSING };

/* RFC 6455 close status codes */
enum wsCloseCode {
  WS_CLOSE_NORMAL = 1000,
  WS_CLOSE_GOING_AWAY = 1001,
  WS_CLOSE_PROTOCOL = 1002,
  WS_CLOSE_UNSUPPORTED = 1003,
  WS_CLOSE_NO_STATUS = 1005, /* must not be sent on wire */
  WS_CLOSE_ABNORMAL = 1006,  /* must not be sent on wire */
  WS_CLOSE_INVALID_DATA = 1007,
  WS_CLOSE_POLICY = 1008,
  WS_CLOSE_TOO_LARGE = 1009,
  WS_CLOSE_EXTENSION = 1010,
  WS_CLOSE_UNEXPECTED = 1011
};

/* HTTP Upgrade handshake */
struct handshake {
  char *host;
  char *origin;
  char *key;
  char *resource;
  enum wsFrameType frameType;
};

/* Initialize handshake struct to NULL/empty */
void nullHandshake(struct handshake *hs);

/* Free all handshake-owned memory and reset */
void freeHandshake(struct handshake *hs);

/* Parse HTTP Upgrade request into handshake */
enum wsFrameType wsParseHandshake(const uint8_t *inputFrame, size_t inputLength,
                                  struct handshake *hs);

/* Build HTTP 101 Switching Protocols response */
void wsGetHandshakeAnswer(const struct handshake *hs, uint8_t *outFrame,
                          size_t *outLength);

/* ---- Frame building ---- */

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

/*
 * Parse the payload of a received CLOSE frame.
 * outCode receives the status code (or WS_CLOSE_NO_STATUS if payload < 2).
 * outReason receives a pointer into payload for the reason string (may be
 * empty). outReasonLen receives the reason length in bytes.
 * Returns FALSE if the payload is malformed (payload > 0 but < 2 bytes).
 */
int wsParseCloseFrame(const uint8_t *payload, size_t payloadLen,
                      enum wsCloseCode *outCode, const char **outReason,
                      size_t *outReasonLen);

/* ---- Single-frame parsing ---- */

/* Parse a WebSocket frame in-place, unmasking payload (stateless) */
enum wsFrameType wsParseInputFrame(uint8_t *inputFrame, size_t inputLength,
                                   uint8_t **dataPtr, size_t *dataLength);

/* ---- Continuation-frame message assembly ---- */

struct wsMessageContext {
  uint8_t *buffer;
  size_t capacity;
  size_t length;
  uint8_t opcode;  /* WS_TEXT_FRAME or WS_BINARY_FRAME */
  int in_progress; /* 0 = idle, 1 = assembling */
};

void wsInitMessageContext(struct wsMessageContext *ctx, uint8_t *buffer,
                          size_t capacity);

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

/* ---- Streaming callbacks (no buffering) ---- */

typedef void (*ws_on_message_begin_cb)(uint8_t opcode,
                                       size_t frame_payload_length, void *user);

typedef void (*ws_on_message_data_cb)(const uint8_t *data, size_t len,
                                      void *user);

typedef void (*ws_on_message_end_cb)(void *user);

struct wsStreamCallbacks {
  ws_on_message_begin_cb on_begin;
  ws_on_message_data_cb on_data;
  ws_on_message_end_cb on_end;
  void *user;
  /* internal state — zero-initialise, do not touch */
  uint8_t _opcode;
  int _in_progress;
};

/*
 * Parse one WebSocket frame and dispatch to streaming callbacks.
 * For fragmented messages, on_begin fires once per fragment with that
 * fragment's payload length. Returns the frame type; WS_INCOMPLETE_FRAME
 * for non-final continuation frames.
 */
enum wsFrameType wsParseInputFrameStream(uint8_t *inputFrame,
                                         size_t inputLength,
                                         struct wsStreamCallbacks *cbs);

/* ---- Buffer consumer (TCP stream integration) ---- */

typedef void (*ws_on_message_cb)(enum wsFrameType type, uint8_t *data,
                                 size_t len, void *user);

typedef void (*ws_on_control_cb)(enum wsFrameType type, uint8_t *data,
                                 size_t len, void *user);

/*
 * Walk a TCP buffer that may contain multiple concatenated frames,
 * dispatching each to wsParseInputFrameWithContext.
 *
 * buf        : raw receive buffer (modified in-place for unmasking)
 * bufLen     : number of valid bytes in buf
 * ctx        : message-assembly context (caller owns)
 * on_message : called for each complete message with its type, data, length
 * on_control : called for each control frame (PING/PONG/CLOSE) with payload
 * user       : passed through to both callbacks
 *
 * Returns the number of bytes consumed. If a partial frame is at the end,
 * the caller should move the unconsumed bytes to the front of the buffer
 * and append the next recv() result before calling again.
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

#endif /* WEBSOCKET_H */
