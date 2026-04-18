#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include "../lib/websocket.h"

/* ---- Echo server streaming callbacks ---- */

struct echo_ctx {
    int     client_fd;
    int     error;
    uint8_t outbuf[4096];
};

static void on_begin(uint8_t opcode, size_t total, void *u)
{
    (void)opcode;
    (void)total;
    (void)u;
}

static void on_data(const uint8_t *data, size_t len, void *u)
{
    struct echo_ctx *ctx = (struct echo_ctx *)u;
    size_t out_len = sizeof(ctx->outbuf);
    wsMakeFrame(data, len, ctx->outbuf, &out_len, WS_TEXT_FRAME);
    if (send(ctx->client_fd, ctx->outbuf, out_len, 0) < 0)
        ctx->error = 1;
}

static void on_end(void *u)
{
    (void)u;
}

/* ---- Minimal TCP accept loop ---- */

static int accept_client(int server_fd)
{
    struct sockaddr_in addr;
    socklen_t len = sizeof(addr);
    return accept(server_fd, (struct sockaddr *)&addr, &len);
}

int main(void)
{
    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) {
        perror("socket");
        return 1;
    }

    int yes = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));

    struct sockaddr_in addr = {
        .sin_family = AF_INET,
        .sin_port   = htons(8088),
        .sin_addr   = { htonl(INADDR_ANY) }
    };

    if (bind(server_fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        perror("bind");
        return 1;
    }

    if (listen(server_fd, 1) < 0) {
        perror("listen");
        return 1;
    }

    printf("Streaming WebSocket echo server on ws://localhost:8088/\n");

    for (;;) {
        int client_fd = accept_client(server_fd);
        if (client_fd < 0) {
            perror("accept");
            continue;
        }

        printf("Client connected\n");

        uint8_t buf[4096];
        ssize_t n = recv(client_fd, buf, sizeof(buf), 0);
        if (n <= 0) {
            close(client_fd);
            continue;
        }

        /* ---- Parse handshake ---- */
        struct handshake hs;
        nullHandshake(&hs);
        enum wsFrameType t = wsParseHandshake(buf, (size_t)n, &hs);
        if (t != WS_OPENING_FRAME) {
            printf("Bad handshake\n");
            freeHandshake(&hs);
            close(client_fd);
            continue;
        }

        uint8_t reply[1024];
        size_t reply_len = sizeof(reply);
        wsGetHandshakeAnswer(&hs, reply, &reply_len);
        freeHandshake(&hs);

        if (send(client_fd, reply, reply_len, 0) < 0) {
            close(client_fd);
            continue;
        }

        /* ---- Streaming callbacks ---- */
        struct echo_ctx ctx = {
            .client_fd = client_fd,
            .error     = 0
        };

        struct wsStreamCallbacks cbs = {
            .on_begin = on_begin,
            .on_data  = on_data,
            .on_end   = on_end,
            .user     = &ctx
        };

        /* ---- Frame loop ---- */
        for (;;) {
            ssize_t r = recv(client_fd, buf, sizeof(buf), 0);
            if (r <= 0) {
                printf("Client disconnected\n");
                break;
            }

            enum wsFrameType ft =
                wsParseInputFrameStream(buf, (size_t)r, &cbs);

            if (ft == WS_PING_FRAME) {
                size_t out_len = sizeof(ctx.outbuf);
                wsMakeFrame(NULL, 0, ctx.outbuf, &out_len, WS_PONG_FRAME);
                if (send(client_fd, ctx.outbuf, out_len, 0) < 0)
                    break;
                continue;
            }

            if (ft == WS_CLOSING_FRAME) {
                size_t out_len = sizeof(ctx.outbuf);
                wsMakeFrame(NULL, 0, ctx.outbuf, &out_len, WS_CLOSING_FRAME);
                send(client_fd, ctx.outbuf, out_len, 0); /* best-effort */
                break;
            }

            if (ctx.error) {
                printf("Send error\n");
                break;
            }

            if (ft == WS_ERROR_FRAME) {
                printf("Protocol error\n");
                break;
            }
        }

        close(client_fd);
    }

    close(server_fd);
    return 0;
}
