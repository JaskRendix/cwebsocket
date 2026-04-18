#define _GNU_SOURCE
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <unistd.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <sys/wait.h>

#include "../lib/websocket.h"

static int connect_to_server(void)
{
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    assert(fd >= 0);

    struct sockaddr_in addr = {
        .sin_family = AF_INET,
        .sin_port   = htons(8088),
        .sin_addr   = { htonl(INADDR_LOOPBACK) }
    };

    int r = connect(fd, (struct sockaddr *)&addr, sizeof(addr));
    assert(r == 0);

    return fd;
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
    ssize_t r = recv(fd, buf, n, 0);
    assert(r >= 0);
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

    assert(n >= 0);
    assert(memmem(buf, (size_t)n, "101 Switching Protocols", 23) != NULL);
}

static void test_echo(int fd)
{
    uint8_t frame[64];
    size_t flen = sizeof(frame);

    wsMakeFrame((const uint8_t *)"Hi", 2, frame, &flen, WS_TEXT_FRAME);
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
    size_t flen = sizeof(frame);

    wsMakeFrame(NULL, 0, frame, &flen, WS_PING_FRAME);
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
    size_t flen = sizeof(frame);

    wsMakeFrame(NULL, 0, frame, &flen, WS_CLOSING_FRAME);
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

    sleep(1); /* give server time to start */

    int fd = connect_to_server();

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
