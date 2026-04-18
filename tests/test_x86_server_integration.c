#define _GNU_SOURCE
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <unistd.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <sys/wait.h>
#include "../lib/websocket.h"

static size_t make_masked_frame(uint8_t *out, size_t out_size,
                                const uint8_t *payload, size_t len,
                                enum wsFrameType type)
{
    static const uint8_t mask[4] = {0x37, 0xfa, 0x21, 0x3d};
    assert(out_size >= len + 10);
    size_t p = 0;
    out[p++] = 0x80 | (uint8_t)type;
    out[p++] = 0x80 | (uint8_t)len;  /* mask bit set, len <= 125 assumed */
    memcpy(&out[p], mask, 4);
    p += 4;
    for (size_t i = 0; i < len; i++)
        out[p++] = payload ? (payload[i] ^ mask[i % 4]) : mask[i % 4];
    return p;
}

static void send_all(int fd, const void *buf, size_t n)
{
    const uint8_t *p = buf;
    while (n > 0) {
        ssize_t w = send(fd, p, n, 0);
        assert(w > 0);
        p += (size_t)w;
        n -= (size_t)w;
    }
}

static ssize_t recv_some(int fd, uint8_t *buf, size_t n)
{
    struct timeval tv = { .tv_sec = 3, .tv_usec = 0 };
    setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
    ssize_t r = recv(fd, buf, n, 0);
    assert(r > 0);
    return r;
}

static void test_handshake(int fd)
{
    const char req[] =
        "GET /echo HTTP/1.1\r\n"
        "Host: localhost\r\n"
        "Upgrade: websocket\r\n"
        "Connection: Upgrade\r\n"
        "Sec-WebSocket-Key: dGhlIHNhbXBsZSBub25jZQ==\r\n"
        "Sec-WebSocket-Version: 13\r\n\r\n";
    send_all(fd, req, strlen(req));
    uint8_t buf[1024];
    ssize_t n = recv_some(fd, buf, sizeof(buf));
    assert(n > 0);
    assert(memmem(buf, (size_t)n, "101 Switching Protocols", 23) != NULL);
}

static void test_echo(int fd)
{
    uint8_t frame[64];
    size_t flen = make_masked_frame(frame, sizeof(frame),
                                    (const uint8_t *)"Hi", 2, WS_TEXT_FRAME);
    send_all(fd, frame, flen);

    uint8_t buf[64];
    ssize_t n = recv_some(fd, buf, sizeof(buf));

    uint8_t expected[64];
    size_t elen = sizeof(expected);
    wsMakeFrame((const uint8_t *)"Hi", 2, expected, &elen, WS_TEXT_FRAME);
    assert(n == (ssize_t)elen);
    assert(memcmp(buf, expected, elen) == 0);
}

static void test_ping_pong(int fd)
{
    uint8_t frame[32];
    size_t flen = make_masked_frame(frame, sizeof(frame),
                                    NULL, 0, WS_PING_FRAME);
    send_all(fd, frame, flen);

    uint8_t buf[32];
    ssize_t n = recv_some(fd, buf, sizeof(buf));

    uint8_t expected[32];
    size_t elen = sizeof(expected);
    wsMakeFrame(NULL, 0, expected, &elen, WS_PONG_FRAME);
    assert(n == (ssize_t)elen);
    assert(memcmp(buf, expected, elen) == 0);
}

static void test_close(int fd)
{
    uint8_t frame[32];
    size_t flen = make_masked_frame(frame, sizeof(frame),
                                    NULL, 0, WS_CLOSING_FRAME);
    send_all(fd, frame, flen);

    uint8_t buf[32];
    ssize_t n = recv_some(fd, buf, sizeof(buf));

    uint8_t expected[32];
    size_t elen = sizeof(expected);
    wsMakeFrame(NULL, 0, expected, &elen, WS_CLOSING_FRAME);
    assert(n == (ssize_t)elen);
    assert(memcmp(buf, expected, elen) == 0);
}

int main(void)
{
    printf("Starting x86 server integration test...\n");

    pid_t pid = fork();
    assert(pid >= 0);
    if (pid == 0) {
        execl("./x86_server", "./x86_server", NULL);
        _exit(1);
    }

    int fd = -1;
    for (int i = 0; i < 20; i++) {
        usleep(100000);  /* 100ms */
        fd = socket(AF_INET, SOCK_STREAM, 0);
        struct sockaddr_in addr = {
            .sin_family = AF_INET,
            .sin_port   = htons(8088),
            .sin_addr   = { htonl(INADDR_LOOPBACK) }
        };
        if (connect(fd, (struct sockaddr *)&addr, sizeof(addr)) == 0)
            break;
        close(fd);
        fd = -1;
    }
    assert(fd >= 0);

    test_handshake(fd);
    test_echo(fd);
    test_ping_pong(fd);
    test_close(fd);

    close(fd);
    kill(pid, SIGTERM);
    waitpid(pid, NULL, 0);

    printf("x86 server integration test passed.\n");
    return 0;
}
