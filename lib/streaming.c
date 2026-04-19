/*
 * Copyright (c) 2014 Putilov Andrey
 *
 * MIT License
 */

#include "streaming.h"
#include "frame_parser.h"

#include <assert.h>
#include <string.h>

enum wsFrameType wsParseInputFrameStream(uint8_t *inputFrame,
                                         size_t inputLength,
                                         struct wsStreamCallbacks *cbs) {
  uint8_t *payload = NULL;
  size_t plen = 0;

  /* First parse the frame normally (opcode, mask, RSV, payload, etc.) */
  enum wsFrameType t =
      wsParseInputFrameSingle(inputFrame, inputLength, &payload, &plen);

  if (t == WS_INCOMPLETE_FRAME || t == WS_ERROR_FRAME)
    return t;

  uint8_t fin = (inputFrame[0] & 0x80u) != 0;
  uint8_t opcode = (inputFrame[0] & 0x0Fu);

  /* ---- Control frames (PING, PONG, CLOSE) ---- */
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

  /* ---- Data frames (TEXT, BINARY) ---- */
  if (opcode == WS_TEXT_FRAME || opcode == WS_BINARY_FRAME) {
    cbs->_opcode = opcode;
    cbs->_in_progress = !fin;

    if (cbs && cbs->on_begin)
      cbs->on_begin(opcode, plen, cbs->user);

    if (cbs && cbs->on_data && plen > 0)
      cbs->on_data(payload, plen, cbs->user);

    if (fin && cbs && cbs->on_end)
      cbs->on_end(cbs->user);

    return t;
  }

  /* ---- Continuation frames ---- */
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
