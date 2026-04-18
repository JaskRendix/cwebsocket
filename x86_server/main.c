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
 *
 */

#include <arpa/inet.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#include "websocket.h"

#define PORT 8088
#define BUF_LEN 0xFFFF
#define PACKET_DUMP

struct echo_ctx {
  int client_fd;
  int error;
  uint8_t outbuf[BUF_LEN];
};

static void error(const char *msg) {
  perror(msg);
  exit(EXIT_FAILURE);
}

static int safeSend(int clientSocket, const uint8_t *buffer,
                    size_t bufferSize) {
#ifdef PACKET_DUMP
  printf("out packet:\n");
  fwrite(buffer, 1, bufferSize, stdout);
  printf("\n");
#endif
  ssize_t written = send(clientSocket, buffer, bufferSize, 0);
  if (written == -1) {
    close(clientSocket);
    perror("send failed");
    return EXIT_FAILURE;
  }
  if ((size_t)written != bufferSize) {
    close(clientSocket);
    perror("written not all bytes");
    return EXIT_FAILURE;
  }

  return EXIT_SUCCESS;
}

/* streaming callbacks: echo each data chunk as TEXT frame */

static void echo_on_begin(uint8_t opcode, size_t total_length, void *user) {
  (void)opcode;
  (void)total_length;
  (void)user;
}

static void echo_on_data(const uint8_t *data, size_t len, void *user) {
  struct echo_ctx *ctx = (struct echo_ctx *)user;

  size_t out_len = sizeof(ctx->outbuf);
  wsMakeFrame(data, len, ctx->outbuf, &out_len, WS_TEXT_FRAME);
  if (safeSend(ctx->client_fd, ctx->outbuf, out_len) == EXIT_FAILURE)
    ctx->error = 1;
}

static void echo_on_end(void *user) { (void)user; }

static void clientWorker(int clientSocket) {
  uint8_t buf[BUF_LEN];
  ssize_t n;

  /* ---- Handshake ---- */
  struct handshake hs;
  nullHandshake(&hs);

  n = recv(clientSocket, buf, sizeof(buf), 0);
  if (n <= 0) {
    close(clientSocket);
    return;
  }

#ifdef PACKET_DUMP
  printf("in packet (handshake):\n");
  fwrite(buf, 1, (size_t)n, stdout);
  printf("\n");
#endif

  enum wsFrameType ht = wsParseHandshake(buf, (size_t)n, &hs);
  if (ht != WS_OPENING_FRAME) {
    /* simple 400 response */
    const char bad[] = "HTTP/1.1 400 Bad Request\r\n\r\n";
    safeSend(clientSocket, (const uint8_t *)bad, strlen(bad));
    freeHandshake(&hs);
    close(clientSocket);
    return;
  }

  /* optional resource check, keep /echo for compatibility */
  if (!hs.resource || strcmp(hs.resource, "/echo") != 0) {
    const char not_found[] = "HTTP/1.1 404 Not Found\r\n\r\n";
    safeSend(clientSocket, (const uint8_t *)not_found, strlen(not_found));
    freeHandshake(&hs);
    close(clientSocket);
    return;
  }

  uint8_t reply[1024];
  _Static_assert(sizeof(reply) >= 256, "reply buffer too small for handshake");
  size_t reply_len = sizeof(reply);
  wsGetHandshakeAnswer(&hs, reply, &reply_len);
  freeHandshake(&hs);

  if (safeSend(clientSocket, reply, reply_len) == EXIT_FAILURE) {
    close(clientSocket);
    return;
  }

  /* ---- Streaming echo context ---- */

  struct echo_ctx ctx;
  ctx.client_fd = clientSocket;
  ctx.error = 0;

  struct wsStreamCallbacks cbs;
  cbs.on_begin = echo_on_begin;
  cbs.on_data = echo_on_data;
  cbs.on_end = echo_on_end;
  cbs.user = &ctx;

  /* ---- Frame loop ---- */

  for (;;) {
    n = recv(clientSocket, buf, sizeof(buf), 0);
    if (n <= 0) {
      break;
    }

#ifdef PACKET_DUMP
    printf("in packet (data):\n");
    fwrite(buf, 1, (size_t)n, stdout);
    printf("\n");
#endif

    enum wsFrameType ft = wsParseInputFrameStream(buf, (size_t)n, &cbs);

    if (ft == WS_CLOSING_FRAME) {
      size_t out_len = sizeof(ctx.outbuf);
      wsMakeFrame(NULL, 0, ctx.outbuf, &out_len, WS_CLOSING_FRAME);
      safeSend(clientSocket, ctx.outbuf, out_len);
      break;
    }

    if (ft == WS_PING_FRAME) {
      size_t out_len = sizeof(ctx.outbuf);
      wsMakeFrame(NULL, 0, ctx.outbuf, &out_len, WS_PONG_FRAME);
      safeSend(clientSocket, ctx.outbuf, out_len);
    }

    if (ctx.error) {
      break;
    }

    if (ft == WS_ERROR_FRAME) {
      size_t out_len = sizeof(ctx.outbuf);
      wsMakeFrame(NULL, 0, ctx.outbuf, &out_len, WS_CLOSING_FRAME);
      safeSend(clientSocket, ctx.outbuf, out_len);
      break;
    }
  }

  close(clientSocket);
}

int main(int argc, char **argv) {
  (void)argc;
  (void)argv;

  int listenSocket = socket(AF_INET, SOCK_STREAM, 0);
  if (listenSocket == -1) {
    error("create socket failed");
  }

  int yes = 1;
  if (setsockopt(listenSocket, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes)) <
      0) {
    error("setsockopt failed");
  }

  struct sockaddr_in local;
  memset(&local, 0, sizeof(local));
  local.sin_family = AF_INET;
  local.sin_addr.s_addr = INADDR_ANY;
  local.sin_port = htons(PORT);

  if (bind(listenSocket, (struct sockaddr *)&local, sizeof(local)) == -1) {
    error("bind failed");
  }

  if (listen(listenSocket, 1) == -1) {
    error("listen failed");
  }

  printf("opened %s:%d\n", inet_ntoa(local.sin_addr), ntohs(local.sin_port));

  while (1) {
    struct sockaddr_in remote;
    socklen_t sockaddrLen = sizeof(remote);
    int clientSocket =
        accept(listenSocket, (struct sockaddr *)&remote, &sockaddrLen);
    if (clientSocket == -1) {
      error("accept failed");
    }

    printf("connected %s:%d\n", inet_ntoa(remote.sin_addr),
           ntohs(remote.sin_port));
    clientWorker(clientSocket);
    printf("disconnected\n");
  }

  close(listenSocket);
  return EXIT_SUCCESS;
}
