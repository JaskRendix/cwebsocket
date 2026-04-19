/*
 * Copyright (c) 2013 Putilov Andrey
 *
 * MIT License
 *
 */

#define DEBUG
#define PORT 8088
#define BUF_LEN 255  /* Safety margin for headers + payload */

#ifdef DEBUG
  #define __ASSERT_USE_STDERR
#endif

#include <SPI.h>
#include <Ethernet.h>
#include <websocket.h>

/* Network configuration */
byte mac[] = { 0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED };
IPAddress ip(192, 168, 0, 4);
EthernetServer server(PORT);

/* Redirect printf to Serial */
int serialWrite(char c, FILE *f) {
  Serial.write(c);
  return 0;
}

void setup() {
  Ethernet.begin(mac, ip);
  server.begin();

#ifdef DEBUG
  Serial.begin(9600);
  stdout = stderr = fdevopen(serialWrite, NULL);
  printf_P(PSTR("Server started. Waiting for clients...\n"));
#endif
}

void clientWorker(EthernetClient client) {
  uint8_t buffer[BUF_LEN];
  size_t readedLength = 0;

  enum wsState state = WS_STATE_OPENING;
  enum wsFrameType frameType = WS_INCOMPLETE_FRAME;

  uint8_t *data = NULL;
  size_t dataSize = 0;

  struct handshake hs;
  nullHandshake(&hs);

  /* Fragmentation reassembly context */
  struct wsMessageContext ctx;
  uint8_t msgBuffer[BUF_LEN];
  wsInitMessageContext(&ctx, msgBuffer, BUF_LEN);

  while (client.connected()) {

    /* 1. Read available bytes */
    while (client.available() && readedLength < BUF_LEN) {
      buffer[readedLength++] = client.read();

      /* 2. Try parsing */
      if (state == WS_STATE_OPENING) {
        frameType = wsParseHandshake(buffer, readedLength, &hs);
      } else {
        frameType = wsParseInputFrameWithContext(
            buffer, readedLength, &data, &dataSize, &ctx);
      }

      if (frameType != WS_INCOMPLETE_FRAME)
        break;
    }

    /* 3. Error handling */
    if (frameType == WS_ERROR_FRAME) {
#ifdef DEBUG
      printf_P(PSTR("Protocol error or invalid frame\n"));
#endif
      break;
    }

    /* 4. Handshake */
    if (state == WS_STATE_OPENING && frameType == WS_OPENING_FRAME) {

      if (strcmp(hs.resource, "/echo") == 0) {
        size_t outLen = BUF_LEN;
        wsGetHandshakeAnswer(&hs, buffer, &outLen);
        client.write(buffer, outLen);
        state = WS_STATE_NORMAL;

#ifdef DEBUG
        printf_P(PSTR("Handshake successful\n"));
#endif

      } else {
        client.write("HTTP/1.1 404 Not Found\r\n\r\n");
        break;
      }

      freeHandshake(&hs);
      readedLength = 0;
      frameType = WS_INCOMPLETE_FRAME;
    }

    /* 5. WebSocket frame handling */
    else if (state == WS_STATE_NORMAL && frameType != WS_INCOMPLETE_FRAME) {

      size_t outLen = BUF_LEN;

      switch (frameType) {

        case WS_TEXT_FRAME:
        case WS_BINARY_FRAME:
          wsMakeFrame(data, dataSize, buffer, &outLen, frameType);
          client.write(buffer, outLen);
          break;

        case WS_PING_FRAME:
          wsMakePongFrame(data, dataSize, buffer, &outLen);
          client.write(buffer, outLen);
#ifdef DEBUG
          printf_P(PSTR("Ping received, Pong sent\n"));
#endif
          break;

        case WS_CLOSING_FRAME:
          wsMakeFrame(NULL, 0, buffer, &outLen, WS_CLOSING_FRAME);
          client.write(buffer, outLen);
          goto disconnect;

        default:
          break;
      }

      readedLength = 0;
      frameType = WS_INCOMPLETE_FRAME;
    }

    /* Buffer overflow protection */
    if (readedLength >= BUF_LEN && frameType == WS_INCOMPLETE_FRAME) {
#ifdef DEBUG
      printf_P(PSTR("Buffer overflow: Frame too large\n"));
#endif
      break;
    }

    delay(1);
  }

disconnect:
#ifdef DEBUG
  printf_P(PSTR("Closing connection\n"));
#endif
  client.stop();
}

void loop() {
  EthernetClient client = server.available();
  if (client) {
#ifdef DEBUG
    printf_P(PSTR("Client connected\n"));
#endif
    clientWorker(client);
  }
}
