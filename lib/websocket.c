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

#include "websocket.h"

#include <assert.h>
#include <ctype.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const char rn[] = "\r\n";

/* Utility: safe lowercase in-place */
static void str_to_lower(char *s) {
  if (!s)
    return;
  for (; *s; ++s) {
    unsigned char ch = (unsigned char)*s;
    *s = (char)tolower((int)ch);
  }
}

void nullHandshake(struct handshake *hs) {
  if (!hs)
    return;
  hs->host      = NULL;
  hs->origin    = NULL;
  hs->resource  = NULL;
  hs->key       = NULL;
  hs->frameType = WS_EMPTY_FRAME;
}

void freeHandshake(struct handshake *hs) {
  if (!hs)
    return;
  free(hs->host);
  free(hs->origin);
  free(hs->resource);
  free(hs->key);
  nullHandshake(hs);
}

/* Extract a line up to CRLF, allocate and return a NUL-terminated copy */
static char *getUptoLinefeed(const char *startFrom) {
  if (!startFrom)
    return NULL;

  const char *lf = strstr(startFrom, rn);
  if (!lf)
    return NULL;

  size_t newLength = (size_t)(lf - startFrom);
  if (newLength == 0) {
    char *empty = (char *)malloc(1);
    if (!empty)
      return NULL;
    empty[0] = '\0';
    return empty;
  }

  char *writeTo = (char *)malloc(newLength + 1);
  if (!writeTo)
    return NULL;

  memcpy(writeTo, startFrom, newLength);
  writeTo[newLength] = '\0';
  return writeTo;
}

enum wsFrameType wsParseHandshake(const uint8_t *inputFrame, size_t inputLength,
                                  struct handshake *hs) {
  if (!inputFrame || !hs)
    return WS_ERROR_FRAME;

  const char *inputPtr = (const char *)inputFrame;
  const char *endPtr   = (const char *)inputFrame + inputLength;

  if (!strstr(inputPtr, "\r\n\r\n"))
    return WS_INCOMPLETE_FRAME;

  if (inputLength < 4 || strncmp(inputPtr, "GET ", 4) != 0)
    return WS_ERROR_FRAME;

  const char *first = strchr(inputPtr, ' ');
  if (!first)
    return WS_ERROR_FRAME;
  ++first;
  const char *second = strchr(first, ' ');
  if (!second || second <= first)
    return WS_ERROR_FRAME;

  size_t resourceLen = (size_t)(second - first);
  free(hs->resource);
  hs->resource = (char *)malloc(resourceLen + 1);
  if (!hs->resource)
    return WS_ERROR_FRAME;
  memcpy(hs->resource, first, resourceLen);
  hs->resource[resourceLen] = '\0';

  const char *line = strstr(inputPtr, rn);
  if (!line)
    return WS_ERROR_FRAME;
  line += 2;

  uint8_t connectionFlag   = FALSE;
  uint8_t upgradeFlag      = FALSE;
  uint8_t subprotocolFlag  = FALSE;
  uint8_t versionMismatch  = FALSE;

  free(hs->host);   hs->host   = NULL;
  free(hs->origin); hs->origin = NULL;
  free(hs->key);    hs->key    = NULL;

  while (line < endPtr && !(line[0] == '\r' && line[1] == '\n')) {
    if (strncmp(line, hostField, strlen(hostField)) == 0) {
      line += strlen(hostField);
      hs->host = getUptoLinefeed(line);
    } else if (strncmp(line, originField, strlen(originField)) == 0) {
      line += strlen(originField);
      hs->origin = getUptoLinefeed(line);
    } else if (strncmp(line, protocolField, strlen(protocolField)) == 0) {
      line += strlen(protocolField);
      subprotocolFlag = TRUE;
    } else if (strncmp(line, keyField, strlen(keyField)) == 0) {
      line += strlen(keyField);
      hs->key = getUptoLinefeed(line);
    } else if (strncmp(line, versionField, strlen(versionField)) == 0) {
      line += strlen(versionField);
      char *versionString = getUptoLinefeed(line);
      if (!versionString)
        return WS_ERROR_FRAME;
      if (strncmp(versionString, version, strlen(version)) != 0)
        versionMismatch = TRUE;
      free(versionString);
    } else if (strncmp(line, connectionField, strlen(connectionField)) == 0) {
      line += strlen(connectionField);
      char *connectionValue = getUptoLinefeed(line);
      if (!connectionValue)
        return WS_ERROR_FRAME;
      str_to_lower(connectionValue);
      if (strstr(connectionValue, upgrade) != NULL)
        connectionFlag = TRUE;
      free(connectionValue);
    } else if (strncmp(line, upgradeField, strlen(upgradeField)) == 0) {
      line += strlen(upgradeField);
      char *compare = getUptoLinefeed(line);
      if (!compare)
        return WS_ERROR_FRAME;
      str_to_lower(compare);
      if (strncmp(compare, websocket, strlen(websocket)) == 0)
        upgradeFlag = TRUE;
      free(compare);
    }

    const char *next = strstr(line, rn);
    if (!next)
      break;
    line = next + 2;
  }

  if (!hs->host || !hs->key || !connectionFlag || !upgradeFlag ||
      subprotocolFlag || versionMismatch) {
    hs->frameType = WS_ERROR_FRAME;
  } else {
    hs->frameType = WS_OPENING_FRAME;
  }

  return hs->frameType;
}

void wsGetHandshakeAnswer(const struct handshake *hs, uint8_t *outFrame,
                          size_t *outLength) {
  assert(hs);
  assert(outFrame);
  assert(outLength);
  assert(*outLength > 0);
  assert(hs->frameType == WS_OPENING_FRAME);
  assert(hs->key);

  const char *key        = hs->key;
  size_t      key_len    = strlen(key);
  size_t      secret_len = strlen(secret);

  size_t sha_input_len = key_len + secret_len;
  char  *sha_input     = (char *)malloc(sha_input_len + 1);
  if (!sha_input) {
    *outLength = 0;
    return;
  }
  memcpy(sha_input, key, key_len);
  memcpy(sha_input + key_len, secret, secret_len);
  sha_input[sha_input_len] = '\0';

  unsigned char shaHash[SHA1_SIZE];
  memset(shaHash, 0, sizeof(shaHash));
  sha1(shaHash, sha_input, sha_input_len);
  free(sha_input);

  char   accept_key[BASE64LEN(SHA1_SIZE) + 1];
  size_t b64_len = base64(accept_key, sizeof(accept_key), shaHash, SHA1_SIZE);
  if (b64_len == 0 || b64_len >= sizeof(accept_key)) {
    *outLength = 0;
    return;
  }
  accept_key[b64_len] = '\0';

  int written =
      snprintf((char *)outFrame, *outLength,
               "HTTP/1.1 101 Switching Protocols\r\n"
               "%s%s\r\n"
               "%s%s\r\n"
               "Sec-WebSocket-Accept: %s\r\n\r\n",
               upgradeField, websocket, connectionField, upgrade2, accept_key);

  if (written < 0 || (size_t)written >= *outLength) {
    *outLength = 0;
    return;
  }
  *outLength = (size_t)written;
}

/* ---- internal frame header writer (shared by Make and MakeClient) ---- */
static size_t writeFrameHeader(uint8_t *outFrame, uint8_t firstByte,
                               size_t dataLength, int masked) {
  size_t offset = 0;
  outFrame[offset++] = firstByte;

  uint8_t mask_bit = masked ? 0x80u : 0x00u;

  if (dataLength <= 125u) {
    outFrame[offset++] = mask_bit | (uint8_t)dataLength;
  } else if (dataLength <= 0xFFFFu) {
    outFrame[offset++] = mask_bit | 126u;
    uint16_t len16 = htons((uint16_t)dataLength);
    memcpy(&outFrame[offset], &len16, sizeof(len16));
    offset += sizeof(len16);
  } else {
    outFrame[offset++] = mask_bit | 127u;
    uint64_t len64 = htonll((uint64_t)dataLength);
    memcpy(&outFrame[offset], &len64, sizeof(len64));
    offset += sizeof(len64);
  }

  return offset;
}

void wsMakeFrame(const uint8_t *data, size_t dataLength, uint8_t *outFrame,
                 size_t *outLength, enum wsFrameType frameType) {
  assert(outFrame);
  assert(outLength);
  assert(frameType < 0x10);
  if (dataLength > 0)
    assert(data);

  size_t header_len = 2;
  if (dataLength > 125 && dataLength <= 0xFFFFu)
    header_len += 2;
  else if (dataLength > 0xFFFFu)
    header_len += 8;

  assert(*outLength >= header_len + dataLength);

  uint8_t first  = (uint8_t)(0x80u | (uint8_t)frameType);
  size_t  offset = writeFrameHeader(outFrame, first, dataLength, 0);

  if (dataLength > 0) {
    memcpy(&outFrame[offset], data, dataLength);
    offset += dataLength;
  }

  *outLength = offset;
}

void wsMakeClientFrame(const uint8_t *data, size_t dataLength,
                       uint8_t *outFrame, size_t *outLength,
                       enum wsFrameType frameType, const uint8_t mask[4]) {
  assert(outFrame);
  assert(outLength);
  assert(frameType < 0x10);
  assert(mask);
  if (dataLength > 0)
    assert(data);

  size_t header_len = 2 + 4; /* base + mask */
  if (dataLength > 125 && dataLength <= 0xFFFFu)
    header_len += 2;
  else if (dataLength > 0xFFFFu)
    header_len += 8;

  assert(*outLength >= header_len + dataLength);

  uint8_t first  = (uint8_t)(0x80u | (uint8_t)frameType);
  size_t  offset = writeFrameHeader(outFrame, first, dataLength, 1);

  /* Write masking key */
  memcpy(&outFrame[offset], mask, 4);
  offset += 4;

  /* Write masked payload */
  for (size_t i = 0; i < dataLength; ++i)
    outFrame[offset++] = data[i] ^ mask[i % 4u];

  *outLength = offset;
}

void wsMakeCloseFrame(enum wsCloseCode code, const char *reason,
                      uint8_t *outFrame, size_t *outLength,
                      const uint8_t mask[4]) {
  assert(outFrame);
  assert(outLength);

  size_t reason_len = reason ? strlen(reason) : 0;
  size_t payload_len = (code != WS_CLOSE_NO_STATUS && code != WS_CLOSE_ABNORMAL)
                           ? 2 + reason_len
                           : 0;

  /* Build raw payload: 2-byte big-endian code + reason */
  uint8_t payload[128];
  assert(payload_len <= sizeof(payload));

  if (payload_len >= 2) {
    payload[0] = (uint8_t)((unsigned int)code >> 8);
    payload[1] = (uint8_t)((unsigned int)code & 0xFFu);
    if (reason_len > 0)
      memcpy(&payload[2], reason, reason_len);
  }

  if (mask) {
    wsMakeClientFrame(payload_len > 0 ? payload : NULL, payload_len,
                      outFrame, outLength, WS_CLOSING_FRAME, mask);
  } else {
    wsMakeFrame(payload_len > 0 ? payload : NULL, payload_len,
                outFrame, outLength, WS_CLOSING_FRAME);
  }
}

int wsParseCloseFrame(const uint8_t *payload, size_t payloadLen,
                      enum wsCloseCode *outCode, const char **outReason,
                      size_t *outReasonLen) {
  assert(outCode);
  assert(outReason);
  assert(outReasonLen);

  if (payloadLen == 0) {
    *outCode      = WS_CLOSE_NO_STATUS;
    *outReason    = NULL;
    *outReasonLen = 0;
    return TRUE;
  }

  if (payloadLen == 1) {
    /* Malformed: code must be 2 bytes */
    *outCode      = WS_CLOSE_PROTOCOL;
    *outReason    = NULL;
    *outReasonLen = 0;
    return FALSE;
  }

  *outCode      = (enum wsCloseCode)(((unsigned int)payload[0] << 8) |
                                      (unsigned int)payload[1]);
  *outReason    = (const char *)(payload + 2);
  *outReasonLen = payloadLen - 2;
  return TRUE;
}

void wsInitMessageContext(struct wsMessageContext *ctx, uint8_t *buffer,
                          size_t capacity) {
  ctx->buffer      = buffer;
  ctx->capacity    = capacity;
  ctx->length      = 0;
  ctx->opcode      = 0;
  ctx->in_progress = 0;
}

void wsResetMessageContext(struct wsMessageContext *ctx) {
  ctx->length      = 0;
  ctx->opcode      = 0;
  ctx->in_progress = 0;
}

static size_t getPayloadLength(const uint8_t *inputFrame, size_t inputLength,
                               uint8_t *payloadFieldExtraBytes,
                               enum wsFrameType *frameType) {
  if (inputLength < 2) {
    *frameType = WS_INCOMPLETE_FRAME;
    return 0;
  }

  uint8_t len7 = (uint8_t)(inputFrame[1] & 0x7Fu);
  *payloadFieldExtraBytes = 0;

  if ((len7 == 0x7Eu && inputLength < 4) ||
      (len7 == 0x7Fu && inputLength < 10)) {
    *frameType = WS_INCOMPLETE_FRAME;
    return 0;
  }

  if (len7 == 0x7Fu && (inputFrame[2] & 0x80u) != 0x0u) {
    *frameType = WS_ERROR_FRAME;
    return 0;
  }

  size_t payloadLength = 0;

  if (len7 == 0x7Eu) {
    uint16_t payloadLength16b = 0;
    *payloadFieldExtraBytes = 2;
    memcpy(&payloadLength16b, &inputFrame[2], *payloadFieldExtraBytes);
    payloadLength = (size_t)ntohs(payloadLength16b);
  } else if (len7 == 0x7Fu) {
    uint64_t payloadLength64b = 0;
    *payloadFieldExtraBytes = 8;
    memcpy(&payloadLength64b, &inputFrame[2], *payloadFieldExtraBytes);
    payloadLength64b = ntohll(payloadLength64b);
    if (payloadLength64b > (uint64_t)SIZE_MAX) {
      *frameType = WS_ERROR_FRAME;
      return 0;
    }
    payloadLength = (size_t)payloadLength64b;
  } else {
    payloadLength = (size_t)len7;
  }

  return payloadLength;
}

/* ---- internal single-frame parser ---- */
static enum wsFrameType wsParseInputFrameSingle(uint8_t *inputFrame,
                                                size_t inputLength,
                                                uint8_t **dataPtr,
                                                size_t *dataLength) {
  assert(inputFrame);
  assert(dataPtr);
  assert(dataLength);

  if (inputLength < 2)
    return WS_INCOMPLETE_FRAME;

  if ((inputFrame[0] & 0x70u) != 0x0u)
    return WS_ERROR_FRAME;

  uint8_t opcode = (uint8_t)(inputFrame[0] & 0x0Fu);

  if ((inputFrame[1] & 0x80u) != 0x80u)
    return WS_ERROR_FRAME;

  if (opcode != WS_TEXT_FRAME && opcode != WS_BINARY_FRAME &&
      opcode != WS_CLOSING_FRAME && opcode != WS_PING_FRAME &&
      opcode != WS_PONG_FRAME && opcode != 0x00)
    return WS_ERROR_FRAME;

  enum wsFrameType frameType = (enum wsFrameType)opcode;

  uint8_t extra        = 0;
  size_t  payloadLength =
      getPayloadLength(inputFrame, inputLength, &extra, &frameType);

  if (frameType == WS_INCOMPLETE_FRAME || frameType == WS_ERROR_FRAME)
    return frameType;

  size_t header_and_mask = 2u + (size_t)extra + 4u;
  if (header_and_mask + payloadLength > inputLength)
    return WS_INCOMPLETE_FRAME;

  uint8_t *maskingKey = &inputFrame[2 + extra];
  *dataPtr   = &inputFrame[2 + extra + 4];
  *dataLength = payloadLength;

  for (size_t i = 0; i < *dataLength; ++i)
    (*dataPtr)[i] ^= maskingKey[i % 4u];

  return frameType;
}

enum wsFrameType wsParseInputFrameWithContext(uint8_t *inputFrame,
                                              size_t inputLength,
                                              uint8_t **dataPtr,
                                              size_t *dataLength,
                                              struct wsMessageContext *ctx) {
  uint8_t *payload = NULL;
  size_t   plen    = 0;

  enum wsFrameType t =
      wsParseInputFrameSingle(inputFrame, inputLength, &payload, &plen);

  if (t == WS_ERROR_FRAME || t == WS_EMPTY_FRAME || t == WS_PING_FRAME ||
      t == WS_PONG_FRAME || t == WS_CLOSING_FRAME || t == WS_OPENING_FRAME) {
    *dataPtr   = payload;
    *dataLength = plen;
    return t;
  }

  uint8_t fin    = (inputFrame[0] & 0x80u) != 0;
  uint8_t opcode = (inputFrame[0] & 0x0Fu);

  /* Unfragmented */
  if ((opcode == WS_TEXT_FRAME || opcode == WS_BINARY_FRAME) && fin == 1 &&
      ctx->in_progress == 0) {
    *dataPtr   = payload;
    *dataLength = plen;
    return t;
  }

  /* Start fragmented */
  if ((opcode == WS_TEXT_FRAME || opcode == WS_BINARY_FRAME) && fin == 0 &&
      ctx->in_progress == 0) {
    if (plen > ctx->capacity)
      return WS_ERROR_FRAME;
    memcpy(ctx->buffer, payload, plen);
    ctx->length      = plen;
    ctx->opcode      = opcode;
    ctx->in_progress = 1;
    return WS_INCOMPLETE_FRAME;
  }

  if (opcode == 0x00 && ctx->in_progress == 0)
    return WS_ERROR_FRAME;

  if ((opcode == WS_TEXT_FRAME || opcode == WS_BINARY_FRAME) &&
      ctx->in_progress == 1)
    return WS_ERROR_FRAME;

  /* Middle continuation */
  if (opcode == 0x00 && fin == 0 && ctx->in_progress == 1) {
    if (ctx->length + plen > ctx->capacity)
      return WS_ERROR_FRAME;
    memcpy(ctx->buffer + ctx->length, payload, plen);
    ctx->length += plen;
    return WS_INCOMPLETE_FRAME;
  }

  /* Final continuation */
  if (opcode == 0x00 && fin == 1 && ctx->in_progress == 1) {
    if (ctx->length + plen > ctx->capacity)
      return WS_ERROR_FRAME;
    memcpy(ctx->buffer + ctx->length, payload, plen);
    ctx->length += plen;
    *dataPtr   = ctx->buffer;
    *dataLength = ctx->length;
    enum wsFrameType final_type =
        (ctx->opcode == WS_TEXT_FRAME) ? WS_TEXT_FRAME : WS_BINARY_FRAME;
    wsResetMessageContext(ctx);
    return final_type;
  }

  return WS_ERROR_FRAME;
}

enum wsFrameType wsParseInputFrame(uint8_t *inputFrame, size_t inputLength,
                                   uint8_t **dataPtr, size_t *dataLength) {
  static uint8_t dummy[1];
  struct wsMessageContext ctx;
  wsInitMessageContext(&ctx, dummy, sizeof(dummy));
  return wsParseInputFrameWithContext(inputFrame, inputLength, dataPtr,
                                      dataLength, &ctx);
}

enum wsFrameType wsParseInputFrameStream(uint8_t *inputFrame,
                                         size_t inputLength,
                                         struct wsStreamCallbacks *cbs) {
  uint8_t *payload = NULL;
  size_t   plen    = 0;

  enum wsFrameType t =
      wsParseInputFrameSingle(inputFrame, inputLength, &payload, &plen);

  if (t == WS_INCOMPLETE_FRAME || t == WS_ERROR_FRAME)
    return t;

  uint8_t fin    = (inputFrame[0] & 0x80u) != 0;
  uint8_t opcode = (inputFrame[0] & 0x0Fu);

  /* Control frames */
  if (opcode == WS_PING_FRAME || opcode == WS_PONG_FRAME ||
      opcode == WS_CLOSING_FRAME) {
    if (cbs && cbs->on_begin)
      cbs->on_begin(opcode, plen, cbs->user);
    if (cbs && cbs->on_data && plen > 0)
      cbs->on_data(payload, plen, cbs->user);
    if (cbs && cbs->on_end)
      cbs->on_end(cbs->user);
    return t;
  }

  /* Data frames */
  if (opcode == WS_TEXT_FRAME || opcode == WS_BINARY_FRAME) {
    cbs->_opcode      = opcode;
    cbs->_in_progress = !fin;
    if (cbs && cbs->on_begin)
      cbs->on_begin(opcode, plen, cbs->user);
    if (cbs && cbs->on_data && plen > 0)
      cbs->on_data(payload, plen, cbs->user);
    if (fin && cbs && cbs->on_end)
      cbs->on_end(cbs->user);
    return t;
  }

  /* Continuation frames */
  if (opcode == 0x00) {
    if (cbs && cbs->on_data && plen > 0)
      cbs->on_data(payload, plen, cbs->user);
    if (fin) {
      if (cbs && cbs->on_end)
        cbs->on_end(cbs->user);
      enum wsFrameType final_type =
          (cbs->_opcode == WS_TEXT_FRAME) ? WS_TEXT_FRAME : WS_BINARY_FRAME;
      cbs->_in_progress = 0;
      return final_type;
    }
    return WS_INCOMPLETE_FRAME;
  }

  return WS_ERROR_FRAME;
}

size_t wsConsumeBuffer(uint8_t *buf, size_t bufLen, struct wsMessageContext *ctx,
                       ws_on_message_cb on_message, ws_on_control_cb on_control,
                       void *user) {
  assert(buf);
  assert(ctx);

  size_t consumed = 0;

  while (consumed < bufLen) {
    uint8_t *frame     = buf + consumed;
    size_t   remaining = bufLen - consumed;

    /* Need at least 2 bytes to determine frame size */
    if (remaining < 2)
      break;

    /* Compute full frame size before parsing */
    uint8_t len7 = (uint8_t)(frame[1] & 0x7Fu);
    size_t  header_len;
    size_t  payload_len;

    if (len7 == 0x7Eu) {
      if (remaining < 4)
        break; /* incomplete header */
      uint16_t l16 = 0;
      memcpy(&l16, &frame[2], 2);
      payload_len = (size_t)ntohs(l16);
      header_len  = 4;
    } else if (len7 == 0x7Fu) {
      if (remaining < 10)
        break;
      uint64_t l64 = 0;
      memcpy(&l64, &frame[2], 8);
      payload_len = (size_t)ntohll(l64);
      header_len  = 10;
    } else {
      payload_len = (size_t)len7;
      header_len  = 2;
    }

    /* Account for masking key if present */
    int masked = (frame[1] & 0x80u) != 0;
    if (masked)
      header_len += 4;

    size_t frame_len = header_len + payload_len;
    if (remaining < frame_len)
      break; /* partial frame — stop and let caller refill */

    uint8_t *dataPtr   = NULL;
    size_t   dataLength = 0;

    enum wsFrameType t =
        wsParseInputFrameWithContext(frame, frame_len, &dataPtr, &dataLength,
                                     ctx);

    if (t == WS_ERROR_FRAME) {
      /* Unrecoverable — signal by returning 0 unconsumed */
      return 0;
    }

    if (t == WS_PING_FRAME || t == WS_PONG_FRAME || t == WS_CLOSING_FRAME) {
      if (on_control)
        on_control(t, dataPtr, dataLength, user);
    } else if (t == WS_TEXT_FRAME || t == WS_BINARY_FRAME) {
      if (on_message)
        on_message(t, dataPtr, dataLength, user);
    }
    /* WS_INCOMPLETE_FRAME = fragment in progress, no callback yet */

    consumed += frame_len;
  }

  return consumed;
}
