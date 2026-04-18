#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "../lib/websocket.h"

/* Utility: run a handshake parse and return the frame type */
static enum wsFrameType run(const char *req, struct handshake *hs) {
  nullHandshake(hs);
  return wsParseHandshake((const uint8_t *)req, strlen(req), hs);
}

/* Test: valid handshake */
static void test_valid_handshake(void) {
  struct handshake hs;

  const char *req = "GET /chat HTTP/1.1\r\n"
                    "Host: example.com\r\n"
                    "Upgrade: websocket\r\n"
                    "Connection: Upgrade\r\n"
                    "Sec-WebSocket-Key: dGhlIHNhbXBsZSBub25jZQ==\r\n"
                    "Sec-WebSocket-Version: 13\r\n"
                    "\r\n";

  enum wsFrameType t = run(req, &hs);
  assert(t == WS_OPENING_FRAME);

  assert(strcmp(hs.resource, "/chat") == 0);
  assert(strcmp(hs.host, "example.com") == 0);
  assert(strcmp(hs.key, "dGhlIHNhbXBsZSBub25jZQ==") == 0);

  freeHandshake(&hs);
}

/* Test: missing Host header */
static void test_missing_host(void) {
  struct handshake hs;

  const char *req = "GET /chat HTTP/1.1\r\n"
                    "Upgrade: websocket\r\n"
                    "Connection: Upgrade\r\n"
                    "Sec-WebSocket-Key: abc123==\r\n"
                    "Sec-WebSocket-Version: 13\r\n"
                    "\r\n";

  enum wsFrameType t = run(req, &hs);
  assert(t == WS_ERROR_FRAME);

  freeHandshake(&hs);
}

/* Test: missing Upgrade header */
static void test_missing_upgrade(void) {
  struct handshake hs;

  const char *req = "GET /chat HTTP/1.1\r\n"
                    "Host: example.com\r\n"
                    "Connection: Upgrade\r\n"
                    "Sec-WebSocket-Key: abc123==\r\n"
                    "Sec-WebSocket-Version: 13\r\n"
                    "\r\n";

  enum wsFrameType t = run(req, &hs);
  assert(t == WS_ERROR_FRAME);

  freeHandshake(&hs);
}

/* Test: wrong version */
static void test_wrong_version(void) {
  struct handshake hs;

  const char *req = "GET /chat HTTP/1.1\r\n"
                    "Host: example.com\r\n"
                    "Upgrade: websocket\r\n"
                    "Connection: Upgrade\r\n"
                    "Sec-WebSocket-Key: abc123==\r\n"
                    "Sec-WebSocket-Version: 12\r\n"
                    "\r\n";

  enum wsFrameType t = run(req, &hs);
  assert(t == WS_ERROR_FRAME);

  freeHandshake(&hs);
}

/* Test: incomplete handshake (missing \r\n\r\n) */
static void test_incomplete_handshake(void) {
  struct handshake hs;

  const char *req = "GET /chat HTTP/1.1\r\n"
                    "Host: example.com\r\n";

  enum wsFrameType t = run(req, &hs);
  assert(t == WS_INCOMPLETE_FRAME);

  freeHandshake(&hs);
}

int main(void) {
  printf("Running handshake tests...\n");

  test_valid_handshake();
  test_missing_host();
  test_missing_upgrade();
  test_wrong_version();
  test_incomplete_handshake();

  printf("All handshake tests passed.\n");
  return 0;
}
