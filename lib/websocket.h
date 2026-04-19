/*
 * Copyright (c) 2014 Putilov Andrey
 *
 * MIT License
 */

#ifndef WEBSOCKET_H
#define WEBSOCKET_H

#include <stddef.h>
#include <stdint.h>

/* Versioning */
#define CWEBSOCKET_VERSION_MAJOR 2
#define CWEBSOCKET_VERSION_MINOR 0

/* RFC 6455 constants */
#define WS_MAX_HEADER_SIZE 14

#ifdef __cplusplus
extern "C" {
#endif

#include "aw-base64.h"
#include "aw-sha1.h"
#include <netinet/in.h>

/* 64‑bit host/network byte order helpers */
static inline uint64_t htonll(uint64_t x) {
#if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
  return ((uint64_t)htonl((uint32_t)(x >> 32)) |
          ((uint64_t)htonl((uint32_t)(x & 0xffffffffu)) << 32));
#else
  return x;
#endif
}

static inline uint64_t ntohll(uint64_t x) { return htonll(x); }

/* Boolean flags */
#ifndef TRUE
#define TRUE 1
#endif
#ifndef FALSE
#define FALSE 0
#endif

/* Frame types */
enum wsFrameType {
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

/* Close codes */
enum wsCloseCode {
  WS_CLOSE_NORMAL = 1000,
  WS_CLOSE_GOING_AWAY = 1001,
  WS_CLOSE_PROTOCOL = 1002,
  WS_CLOSE_UNSUPPORTED = 1003,
  WS_CLOSE_NO_STATUS = 1005,
  WS_CLOSE_ABNORMAL = 1006,
  WS_CLOSE_INVALID_DATA = 1007,
  WS_CLOSE_POLICY = 1008,
  WS_CLOSE_TOO_LARGE = 1009,
  WS_CLOSE_EXTENSION = 1010,
  WS_CLOSE_UNEXPECTED = 1011
};

/* ---- Callback typedefs MUST come before wsConsumeBuffer ---- */

typedef void (*ws_on_message_cb)(enum wsFrameType type, uint8_t *data,
                                 size_t len, void *user);

typedef void (*ws_on_control_cb)(enum wsFrameType type, uint8_t *data,
                                 size_t len, void *user);

/* Streaming callbacks */
typedef void (*ws_on_message_begin_cb)(uint8_t opcode,
                                       size_t frame_payload_length, void *user);

typedef void (*ws_on_message_data_cb)(const uint8_t *data, size_t len,
                                      void *user);

typedef void (*ws_on_message_end_cb)(void *user);

/* ---- Handshake ---- */

struct handshake {
  char *host;
  char *origin;
  char *key;
  char *resource;
  enum wsFrameType frameType;
};

void nullHandshake(struct handshake *hs);
void freeHandshake(struct handshake *hs);

enum wsFrameType wsParseHandshake(const uint8_t *inputFrame, size_t inputLength,
                                  struct handshake *hs);

void wsGetHandshakeAnswer(const struct handshake *hs, uint8_t *outFrame,
                          size_t *outLength);

/* ---- Frame building ---- */

void wsMakeFrame(const uint8_t *data, size_t dataLength, uint8_t *outFrame,
                 size_t *outLength, enum wsFrameType frameType);

void wsMakeClientFrame(const uint8_t *data, size_t dataLength,
                       uint8_t *outFrame, size_t *outLength,
                       enum wsFrameType frameType, const uint8_t mask[4]);

void wsMakeCloseFrame(enum wsCloseCode code, const char *reason,
                      uint8_t *outFrame, size_t *outLength,
                      const uint8_t mask[4]);

int wsParseCloseFrame(const uint8_t *payload, size_t payloadLen,
                      enum wsCloseCode *outCode, const char **outReason,
                      size_t *outReasonLen);

/* ---- Frame parsing ---- */

enum wsFrameType wsParseInputFrame(uint8_t *inputFrame, size_t inputLength,
                                   uint8_t **dataPtr, size_t *dataLength);

/* ---- Continuation context ---- */

struct wsMessageContext {
  uint8_t *buffer;
  size_t capacity;
  size_t length;
  uint8_t opcode;
  int in_progress;
};

void wsInitMessageContext(struct wsMessageContext *ctx, uint8_t *buffer,
                          size_t capacity);

void wsResetMessageContext(struct wsMessageContext *ctx);

enum wsFrameType wsParseInputFrameWithContext(uint8_t *inputFrame,
                                              size_t inputLength,
                                              uint8_t **dataPtr,
                                              size_t *dataLength,
                                              struct wsMessageContext *ctx);

/* ---- Streaming ---- */

struct wsStreamCallbacks {
  ws_on_message_begin_cb on_begin;
  ws_on_message_data_cb on_data;
  ws_on_message_end_cb on_end;
  void *user;
  uint8_t _opcode;
  int _in_progress;
};

enum wsFrameType wsParseInputFrameStream(uint8_t *inputFrame,
                                         size_t inputLength,
                                         struct wsStreamCallbacks *cbs);

/* ---- Utility ---- */

static inline size_t wsGetFrameSize(size_t dataLength) {
  size_t header = 2;
  if (dataLength > 65535)
    header += 8;
  else if (dataLength > 125)
    header += 2;
  return header + 4 + dataLength;
}

/* ---- Buffer consumer ---- */

intptr_t wsConsumeBuffer(uint8_t *buf, size_t bufLen,
                         struct wsMessageContext *ctx,
                         ws_on_message_cb on_message,
                         ws_on_control_cb on_control, void *user);

#ifdef __cplusplus
}
#endif

#endif /* WEBSOCKET_H */
