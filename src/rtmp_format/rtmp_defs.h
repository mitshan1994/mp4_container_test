#pragma once

#include <stdint.h>
#include <WinSock2.h>

#define RTMP_BUF_SIZE   (8 * 1024 * 1024)

typedef enum rtmp_msg_type_id_enum
{
    RTMP_MSG_TYPE_AMF0 = 0x14
} rtmp_msg_type_id_enum;

typedef struct rtmp_chunk_stream_t
{
    uint32_t csid;
    uint32_t msid;                  // Set during creation.

    // last chunk info.
    int last_valid;                 // Whether "last info" is valid or inited.
    uint32_t last_fmt;              // in basic header: 0, 1, 2, 3.
    uint32_t last_timestamp_value;
    uint32_t last_msg_stream_id;
    uint32_t last_msg_type_id;
    uint32_t last_msg_length;

    // buffer for message
    uint8_t *msg_buf;
    size_t msg_buf_pos;
    size_t msg_buf_capacity;
} rtmp_chunk_stream_t;

typedef struct rtmp_ctx_t {
    SOCKET s;

    // recv buffer related.
    uint8_t *buf_recv;
    size_t buf_recv_pos;
    size_t buf_recv_capacity;

    // send buffer related.
    uint8_t *buf_send;
    size_t buf_send_pos;
    size_t buf_send_capacity;

    const char *path;   // "live/test".
    char *app;
    char *stream;
    char *tc_url;

    uint32_t recv_max_chunk_size;
    uint32_t send_max_chunk_size;

    // A map of last sent chunk info. Recv chunk streams.
    // New cs is created when receiving first chunk of the chunk stream.
    rtmp_chunk_stream_t *recv_chunk_streams[10];
    size_t recv_cs_count;

    // Sending csid.
    uint32_t csid_chunk_protocol;       // 2
    uint32_t csid_netconnection;        // usually 3. for NetConnection commands.
    uint32_t csid_netstream;            // usually 8. for NetStream commands.
    rtmp_chunk_stream_t *send_chunk_streams[10];
    size_t send_cs_count;

    // clock
    uint64_t self_tick_start; // In milliseconds.
    uint64_t remote_tick_start;

    // handshake.
    uint8_t *c1;
    uint8_t *c2;
    uint8_t *s1;
    uint8_t *s2;
} rtmp_ctx_t;
