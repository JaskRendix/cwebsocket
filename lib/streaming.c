/*
 * Copyright (c) 2014 Putilov Andrey
 *
 * MIT License
 */

#include "streaming.h"
#include "frame_parser.h"

#include <assert.h>
#include <string.h>

/* Validate control frame rules */
static int ws_is_invalid_control_frame(uint8_t fin, size_t plen) {
  if (!fin)
    return 1; /* control frames MUST NOT be fragmented */
  if (plen > 125)
    return 1; /* control frames MUST have <=125 bytes */
  return 0;
}

/* Validate continuation rules */
static int ws_is_invalid_continuation(struct wsStreamCallbacks *cbs) {
  return (cbs->_in_progress == 0);
}

/* Validate start of new data frame */
static int ws_is_invalid_new_data_frame(struct wsStreamCallbacks *cbs) {
  return (cbs->_in_progress != 0);
}

enum wsFrameType wsParseInputFrameStream(uint8_t *inputFrame,
                                         size_t inputLength,
                                         struct wsStreamCallbacks *cbs) {
  assert(cbs != NULL);

  uint8_t *payload = NULL;
  size_t plen = 0;

  enum wsFrameType t =
      wsParseInputFrameSingle(inputFrame, inputLength, &payload, &plen);

  if (t == WS_INCOMPLETE_FRAME || t == WS_ERROR_FRAME)
    return t;

  uint8_t fin = (inputFrame[0] & 0x80u) != 0;
  uint8_t opcode = (inputFrame[0] & 0x0Fu);

  /* ---- CONTROL FRAMES ---- */
  if (opcode == WS_PING_FRAME || opcode == WS_PONG_FRAME ||
      opcode == WS_CLOSING_FRAME) {
    if (ws_is_invalid_control_frame(fin, plen))
      return WS_ERROR_FRAME;

    if (cbs->on_begin)
      cbs->on_begin(opcode, plen, cbs->user);

    if (cbs->on_data && plen > 0)
      cbs->on_data(payload, plen, cbs->user);

    if (cbs->on_end)
      cbs->on_end(cbs->user);

    return t;
  }

  /* ---- DATA FRAMES (TEXT/BINARY) ---- */
  if (opcode == WS_TEXT_FRAME || opcode == WS_BINARY_FRAME) {

    if (ws_is_invalid_new_data_frame(cbs))
      return WS_ERROR_FRAME;

    cbs->_opcode = opcode;
    cbs->_in_progress = !fin;

    if (cbs->on_begin)
      cbs->on_begin(opcode, plen, cbs->user);

    if (cbs->on_data && plen > 0)
      cbs->on_data(payload, plen, cbs->user);

    if (fin && cbs->on_end)
      cbs->on_end(cbs->user);

    return t;
  }

  /* ---- CONTINUATION FRAMES ---- */
  if (opcode == 0x00) {

    if (ws_is_invalid_continuation(cbs))
      return WS_ERROR_FRAME;

    if (cbs->on_data && plen > 0)
      cbs->on_data(payload, plen, cbs->user);

    if (fin) {
      if (cbs->on_end)
        cbs->on_end(cbs->user);

      enum wsFrameType final_type =
          (cbs->_opcode == WS_TEXT_FRAME) ? WS_TEXT_FRAME : WS_BINARY_FRAME;

      cbs->_in_progress = 0;
      return final_type;
    }

    return WS_INCOMPLETE_FRAME;
  }

  /* ---- UNKNOWN OPCODE ---- */
  return WS_ERROR_FRAME;
}
