#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <stdint.h>

#include "../lib/websocket.h"

/* ---- Test callback recorder ---- */

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
    snprintf(buf, sizeof(buf), "DATA(");
    rec(r, buf);
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
    memcpy(&out[p], payload, len);
    return p + len;
}

/* ---- TEST 1: unfragmented text ---- */

static void test_unfragmented_text(void)
{
    uint8_t frame[64];
    size_t f = make_frame(frame, 1, WS_TEXT_FRAME,
                          (const uint8_t *)"Hi", 2);

    struct record r = { .pos = 0 };

    struct wsStreamCallbacks cbs = {
        .on_begin = on_begin,
        .on_data  = on_data,
        .on_end   = on_end,
        .user     = &r
    };

    enum wsFrameType t = wsParseInputFrameStream(frame, f, &cbs);

    assert(t == WS_TEXT_FRAME);
    assert(strcmp(r.log, "BEGIN(1,2);DATA(Hi);END;") == 0);
}

/* ---- TEST 2: fragmented text ---- */

static void test_fragmented_text(void)
{
    uint8_t f1[64], f2[64];
    size_t n1 = make_frame(f1, 0, WS_TEXT_FRAME,
                           (const uint8_t *)"Hel", 3);
    size_t n2 = make_frame(f2, 1, 0x00,
                           (const uint8_t *)"lo", 2);

    struct record r = { .pos = 0 };

    struct wsStreamCallbacks cbs = {
        .on_begin = on_begin,
        .on_data  = on_data,
        .on_end   = on_end,
        .user     = &r
    };

    enum wsFrameType t1 = wsParseInputFrameStream(f1, n1, &cbs);
    enum wsFrameType t2 = wsParseInputFrameStream(f2, n2, &cbs);

    assert(t1 == WS_TEXT_FRAME); /* first frame opcode */
    assert(t2 == WS_TEXT_FRAME); /* final assembled type */

    assert(strcmp(r.log,
                  "BEGIN(1,3);DATA(Hel);DATA(lo);END;") == 0);
}

/* ---- TEST 3: ping frame ---- */

static void test_ping(void)
{
    uint8_t frame[64];
    size_t f = make_frame(frame, 1, WS_PING_FRAME,
                          (const uint8_t *)"X", 1);

    struct record r = { .pos = 0 };

    struct wsStreamCallbacks cbs = {
        .on_begin = on_begin,
        .on_data  = on_data,
        .on_end   = on_end,
        .user     = &r
    };

    enum wsFrameType t = wsParseInputFrameStream(frame, f, &cbs);

    assert(t == WS_PING_FRAME);
    assert(strcmp(r.log, "BEGIN(9,1);DATA(X);END;") == 0);
}

/* ---- TEST 4: fragmented binary ---- */

static void test_fragmented_binary(void)
{
    uint8_t f1[64], f2[64], f3[64];

    uint8_t p1[] = { 0x01, 0x02 };
    uint8_t p2[] = { 0x03 };
    uint8_t p3[] = { 0x04, 0x05 };

    size_t n1 = make_frame(f1, 0, WS_BINARY_FRAME, p1, 2);
    size_t n2 = make_frame(f2, 0, 0x00, p2, 1);
    size_t n3 = make_frame(f3, 1, 0x00, p3, 2);

    struct record r = { .pos = 0 };

    struct wsStreamCallbacks cbs = {
        .on_begin = on_begin,
        .on_data  = on_data,
        .on_end   = on_end,
        .user     = &r
    };

    wsParseInputFrameStream(f1, n1, &cbs);
    wsParseInputFrameStream(f2, n2, &cbs);
    wsParseInputFrameStream(f3, n3, &cbs);

    assert(strcmp(r.log,
                  "BEGIN(2,2);DATA(\x01\x02);DATA(\x03);DATA(\x04\x05);END;") == 0);
}

/* ---- MAIN ---- */

int main(void)
{
    printf("Running streaming tests...\n");

    test_unfragmented_text();
    test_fragmented_text();
    test_ping();
    test_fragmented_binary();

    printf("All streaming tests passed.\n");
    return 0;
}
