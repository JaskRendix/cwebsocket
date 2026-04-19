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

#include "handshake.h"

#include <assert.h>
#include <ctype.h>
#include <stdio.h> /* snprintf */
#include <stdlib.h>
#include <string.h>

/* Internal HTTP/WebSocket header constants */
static const char connectionField[] = "Connection: ";
static const char upgrade[] = "upgrade";
static const char upgrade2[] = "Upgrade";
static const char upgradeField[] = "Upgrade: ";
static const char websocket[] = "websocket";
static const char hostField[] = "Host: ";
static const char originField[] = "Origin: ";
static const char keyField[] = "Sec-WebSocket-Key: ";
static const char protocolField[] = "Sec-WebSocket-Protocol: ";
static const char versionField[] = "Sec-WebSocket-Version: ";
static const char version[] = "13";
static const char secret[] = "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";

static const char rn[] = "\r\n";

/* lowercase utility */
static void str_to_lower(char *s) {
  if (!s)
    return;
  for (; *s; ++s)
    *s = (char)tolower((unsigned char)*s);
}

/* extract substring until CRLF */
static char *getUptoLinefeed(const char *startFrom) {
  if (!startFrom)
    return NULL;
  const char *lf = strstr(startFrom, rn);
  if (!lf)
    return NULL;

  size_t len = (size_t)(lf - startFrom);
  char *out = malloc(len + 1);
  if (!out)
    return NULL;

  memcpy(out, startFrom, len);
  out[len] = '\0';
  return out;
}

void nullHandshake(struct handshake *hs) {
  if (!hs)
    return;
  hs->host = hs->origin = hs->resource = hs->key = NULL;
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

enum wsFrameType wsParseHandshake(const uint8_t *inputFrame, size_t inputLength,
                                  struct handshake *hs) {
  if (!inputFrame || !hs)
    return WS_ERROR_FRAME;

  const char *ptr = (const char *)inputFrame;
  const char *end = ptr + inputLength;

  if (!strstr(ptr, "\r\n\r\n"))
    return WS_INCOMPLETE_FRAME;

  if (inputLength < 4 || strncmp(ptr, "GET ", 4) != 0)
    return WS_ERROR_FRAME;

  /* parse resource */
  const char *first = strchr(ptr, ' ');
  if (!first)
    return WS_ERROR_FRAME;
  first++;
  const char *second = strchr(first, ' ');
  if (!second || second <= first)
    return WS_ERROR_FRAME;

  size_t rlen = (size_t)(second - first);
  free(hs->resource);
  hs->resource = malloc(rlen + 1);
  if (!hs->resource)
    return WS_ERROR_FRAME;
  memcpy(hs->resource, first, rlen);
  hs->resource[rlen] = '\0';

  /* move to first header line */
  const char *line = strstr(ptr, rn);
  if (!line)
    return WS_ERROR_FRAME;
  line += 2;

  uint8_t connectionFlag = FALSE;
  uint8_t upgradeFlag = FALSE;
  uint8_t subprotocolFlag = FALSE;
  uint8_t versionMismatch = FALSE;

  free(hs->host);
  hs->host = NULL;
  free(hs->origin);
  hs->origin = NULL;
  free(hs->key);
  hs->key = NULL;

  while (line < end && !(line[0] == '\r' && line[1] == '\n')) {
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
      char *v = getUptoLinefeed(line);
      if (!v)
        return WS_ERROR_FRAME;
      if (strncmp(v, version, strlen(version)) != 0)
        versionMismatch = TRUE;
      free(v);

    } else if (strncmp(line, connectionField, strlen(connectionField)) == 0) {
      line += strlen(connectionField);
      char *v = getUptoLinefeed(line);
      if (!v)
        return WS_ERROR_FRAME;
      str_to_lower(v);
      if (strstr(v, upgrade) != NULL)
        connectionFlag = TRUE;
      free(v);

    } else if (strncmp(line, upgradeField, strlen(upgradeField)) == 0) {
      line += strlen(upgradeField);
      char *v = getUptoLinefeed(line);
      if (!v)
        return WS_ERROR_FRAME;
      str_to_lower(v);
      if (strncmp(v, websocket, strlen(websocket)) == 0)
        upgradeFlag = TRUE;
      free(v);
    }

    const char *next = strstr(line, rn);
    if (!next)
      break;
    line = next + 2;
  }

  if (!hs->host || !hs->key || !connectionFlag || !upgradeFlag ||
      subprotocolFlag || versionMismatch)
    hs->frameType = WS_ERROR_FRAME;
  else
    hs->frameType = WS_OPENING_FRAME;

  return hs->frameType;
}

void wsGetHandshakeAnswer(const struct handshake *hs, uint8_t *outFrame,
                          size_t *outLength) {
  assert(hs && outFrame && outLength);
  assert(hs->frameType == WS_OPENING_FRAME);
  assert(hs->key);

  size_t key_len = strlen(hs->key);
  size_t secret_len = strlen(secret);
  size_t total = key_len + secret_len;

  char *tmp = malloc(total + 1);
  if (!tmp) {
    *outLength = 0;
    return;
  }

  memcpy(tmp, hs->key, key_len);
  memcpy(tmp + key_len, secret, secret_len);
  tmp[total] = '\0';

  unsigned char shaHash[SHA1_SIZE] = {0};
  sha1(shaHash, tmp, total);
  free(tmp);

  char accept_key[BASE64LEN(SHA1_SIZE) + 1];
  size_t b64 = base64(accept_key, sizeof(accept_key), shaHash, SHA1_SIZE);
  if (b64 == 0 || b64 >= sizeof(accept_key)) {
    *outLength = 0;
    return;
  }
  accept_key[b64] = '\0';

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
