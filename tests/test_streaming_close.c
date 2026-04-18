#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <stdint.h>

#include "../lib/websocket.h"

/* ---- Recorder ---- */

struct record {
    char log[512];
    size_t pos;
};

static void rec(struct record *r, const char *s) {
    size_t n = strlen(s);
    assert(r->pos + n < sizeof(r->log));
    memcpy(r->log + r->pos, s, n);
    r->pos += n;
    r->log[r->pos] = '\0';
}

static void on_begin(uint8_t opcode, size_t total, void *u) {
    struct record *r = (struct record *)u;
    char buf[64];
    snprintf(buf, sizeof(buf), "BEGIN(%u,%zu);", opcode, total);
    rec(r, buf);
}

static void on_data(const uint8_t *d, size_t n, void *u) {
    struct record *r = (struct record *)u;
    char buf[128];
    rec(r, "DATA(");
    for (size_t i = 0; i < n; i++) {
        snprintf(buf, sizeof(buf), "%c", d[i]);
        rec(r, buf);
    }
    rec(r, ");");
}

static void on_end(void *u) {
    struct record *r = (struct record *)u;
    rec(r, "END;");
}

/* ---- Utility: build server-side unmasked frame ---- */

static size_t make_frame(uint8_t *out,
                         uint8_t fin,
                         uint8_t opcode,
                         const uint8_t *payload,
                         size_t len)
{
    static const uint8_t mask[4] = {0x00, 0x00, 0x00, 0x00};
    size_t p = 0;
    out[p++] = (fin ? 0x80 : 0x00) | opcode;
    if (len <= 125) {
        out[p++] = 0x80 | (uint8_t)len;
    } else if (len <= 0xFFFF) {
        out[p++] = 0x80 | 126;
        out[p++] = (uint8_t)(len >> 8);
        out[p++] = (uint8_t)(len & 0xFF);
    } else {
        out[p++] = 0x80 | 127;
        uint64_t v = (uint64_t)len;
        for (int i = 7; i >= 0; i--)
            out[p++] = (uint8_t)((v >> (i * 8)) & 0xFF);
    }
    memcpy(&out[p], mask, 4);
    p += 4;
    memcpy(&out[p], payload, len);  /* all-zero mask = no-op XOR */
    return p + len;
}

/* ---- TEST 1: CLOSE frame with no payload ---- */

static void test_close_empty(void)
{
    uint8_t frame[32];
    size_t f = make_frame(frame, 1, WS_CLOSING_FRAME, NULL, 0);

    struct record r = { .pos = 0 };

    struct wsStreamCallbacks cbs = {
        .on_begin = on_begin,
        .on_data  = on_data,
        .on_end   = on_end,
        .user     = &r
    };

    enum wsFrameType t = wsParseInputFrameStream(frame, f, &cbs);

    assert(t == WS_CLOSING_FRAME);
    assert(strcmp(r.log, "BEGIN(8,0);END;") == 0);
}

/* ---- TEST 2: CLOSE frame with payload (status code + reason) ---- */

static void test_close_with_payload(void)
{
    /* Example payload: 0x03E8 "1000" normal closure + "OK" */
    uint8_t payload[] = { 0x03, 0xE8, 'O', 'K' };

    uint8_t frame[32];
    size_t f = make_frame(frame, 1, WS_CLOSING_FRAME, payload, sizeof(payload));

    struct record r = { .pos = 0 };

    struct wsStreamCallbacks cbs = {
        .on_begin = on_begin,
        .on_data  = on_data,
        .on_end   = on_end,
        .user     = &r
    };

    enum wsFrameType t = wsParseInputFrameStream(frame, f, &cbs);

    assert(t == WS_CLOSING_FRAME);
    assert(strcmp(r.log, "BEGIN(8,4);DATA(\x03\xE8OK);END;") == 0);
}

/* ---- MAIN ---- */

int main(void)
{
    printf("Running streaming CLOSE tests...\n");

    test_close_empty();
    test_close_with_payload();

    printf("All streaming CLOSE tests passed.\n");
    return 0;
}
