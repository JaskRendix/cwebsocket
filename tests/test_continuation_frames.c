#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <stdint.h>

#include "../lib/websocket.h"

/* Utility: build a simple frame (server-side, unmasked) */
static size_t make_frame(uint8_t *out,
                         uint8_t fin,
                         uint8_t opcode,
                         const uint8_t *payload,
                         size_t len)
{
    size_t p = 0;

    out[p++] = (fin ? 0x80 : 0x00) | opcode;

    if (len <= 125) {
        out[p++] = (uint8_t)len;
    } else if (len <= 0xFFFF) {
        out[p++] = 126;
        out[p++] = (uint8_t)(len >> 8);
        out[p++] = (uint8_t)(len & 0xFF);
    } else {
        out[p++] = 127;
        uint64_t v = (uint64_t)len;
        for (int i = 7; i >= 0; i--) {
            out[p++] = (uint8_t)((v >> (i * 8)) & 0xFF);
        }
    }

    memcpy(&out[p], payload, len);
    return p + len;
}

/* Test 1: fragmented text message across two frames */
static void test_fragmented_text_two_frames(void)
{
    uint8_t frame1[64];
    uint8_t frame2[64];

    size_t f1 = make_frame(frame1, 0, WS_TEXT_FRAME,
                           (const uint8_t *)"Hel", 3);

    size_t f2 = make_frame(frame2, 1, 0x00,
                           (const uint8_t *)"lo", 2);

    uint8_t buffer[64];
    struct wsMessageContext ctx;
    wsInitMessageContext(&ctx, buffer, sizeof(buffer));

    uint8_t *data = NULL;
    size_t data_len = 0;

    /* First frame: incomplete */
    enum wsFrameType t1 =
        wsParseInputFrameWithContext(frame1, f1, &data, &data_len, &ctx);

    assert(t1 == WS_INCOMPLETE_FRAME);
    assert(ctx.in_progress == 1);
    assert(ctx.length == 3);
    assert(memcmp(ctx.buffer, "Hel", 3) == 0);

    /* Second frame: completes message */
    enum wsFrameType t2 =
        wsParseInputFrameWithContext(frame2, f2, &data, &data_len, &ctx);

    assert(t2 == WS_TEXT_FRAME);
    assert(data_len == 5);
    assert(memcmp(data, "Hello", 5) == 0);
}

/* Test 2: continuation without start → error */
static void test_continuation_without_start(void)
{
    uint8_t frame[64];
    size_t f = make_frame(frame, 1, 0x00,
                          (const uint8_t *)"x", 1);

    uint8_t buffer[16];
    struct wsMessageContext ctx;
    wsInitMessageContext(&ctx, buffer, sizeof(buffer));

    uint8_t *data = NULL;
    size_t data_len = 0;

    enum wsFrameType t =
        wsParseInputFrameWithContext(frame, f, &data, &data_len, &ctx);

    assert(t == WS_ERROR_FRAME);
}

/* Test 3: new TEXT while fragmented message in progress → error */
static void test_new_text_during_fragment(void)
{
    uint8_t f1buf[64];
    uint8_t f2buf[64];

    size_t f1 = make_frame(f1buf, 0, WS_TEXT_FRAME,
                           (const uint8_t *)"A", 1);

    size_t f2 = make_frame(f2buf, 1, WS_TEXT_FRAME,
                           (const uint8_t *)"B", 1);

    uint8_t buffer[16];
    struct wsMessageContext ctx;
    wsInitMessageContext(&ctx, buffer, sizeof(buffer));

    uint8_t *data = NULL;
    size_t data_len = 0;

    enum wsFrameType t1 =
        wsParseInputFrameWithContext(f1buf, f1, &data, &data_len, &ctx);

    assert(t1 == WS_INCOMPLETE_FRAME);
    assert(ctx.in_progress == 1);

    enum wsFrameType t2 =
        wsParseInputFrameWithContext(f2buf, f2, &data, &data_len, &ctx);

    assert(t2 == WS_ERROR_FRAME);
}

int main(void)
{
    printf("Running continuation frame tests...\n");

    test_fragmented_text_two_frames();
    test_continuation_without_start();
    test_new_text_during_fragment();

    printf("All continuation frame tests passed.\n");
    return 0;
}
