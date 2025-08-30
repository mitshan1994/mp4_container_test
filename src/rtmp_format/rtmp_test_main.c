#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "rtmp_parse_functions.h"
#include "rtmp_defs.h"

int main(int argc, char *argv[])
{
    int ret;
    printf("start rtmp_test_main.\n");

    if (argc < 4) {
        fprintf(stdout, "Usage: %s <ip> <port> <rtmp-path>\n", argv[0]);
        return 1;
    }
    const char *ip = argv[1];
    const char *str_port = argv[2];
    const char *rtmp_path = argv[3];

    rtmp_ctx_t *ctx = malloc(sizeof(*ctx));
    memset(ctx, 0, sizeof(*ctx));

    ctx->s = INVALID_SOCKET;

    ctx->path = rtmp_path;
    char *app_end = strchr(rtmp_path, '/');
    if (NULL == app_end) {
        printf("rtmp-path should have form <app>/<stream>\n");
        return -1;
    }
    ctx->app = malloc(app_end - rtmp_path + 1);
    memcpy(ctx->app, rtmp_path, app_end - rtmp_path);
    ctx->app[app_end - rtmp_path] = '\0';

    size_t stream_len = strlen(rtmp_path) - strlen(ctx->app) - 1;
    ctx->stream = malloc(stream_len + 1);
    memcpy(ctx->stream, app_end + 1, stream_len);
    ctx->stream[stream_len] = '\0';

    ctx->tc_url = malloc(strlen(ctx->app) + strlen(ctx->stream) + 50);
    sprintf(ctx->tc_url, "rtmp://%s:%s/%s", ip, str_port, ctx->app);

    ctx->buf_send = malloc(RTMP_BUF_SIZE);
    ctx->buf_send_capacity = RTMP_BUF_SIZE;
    ctx->buf_recv = malloc(RTMP_BUF_SIZE);
    ctx->buf_recv_capacity = RTMP_BUF_SIZE;

    ctx->send_max_chunk_size = 128;  // default 128.
    ctx->recv_max_chunk_size = 128;

    ctx->csid_chunk_protocol = 2;
    ctx->csid_netconnection = 3;
    ctx->csid_netstream = 8;

    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        return -1;
    }

    // Connect to rtmp server.
    ctx->s = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (ctx->s == INVALID_SOCKET) {
        printf("failed to create tcp socket.\n");
        return -1;
    }

    SOCKADDR_IN addr_in = { 0 };
    addr_in.sin_family = AF_INET;
    addr_in.sin_port = htons(atoi(str_port));
    addr_in.sin_addr.s_addr = inet_addr(ip);

    ret = connect(ctx->s, &addr_in, sizeof(addr_in));
    if (ret != 0) {
        printf("failed to connect tcp\n");
        return -1;
    }
    printf("rtmp connected: %s:%s\n", ip, str_port);

    ret = rtmp_client_parse(ctx);
    if (ret != 0) {
        printf("failed to parse rtmp client connection\n");
        return -1;
    }

    printf("end rtmp_test_main.\n");
    return 0;
}
