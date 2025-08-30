#include "rtmp_parse_functions.h"
#include "read_utils.h"

#include <stdio.h>

#include "AMF.h"

#define C1_SIZE 1536

#define send_data(data, len) rtmp_send(ctx, data, len)
#define recv_data(data, len) rtmp_read(ctx, data, len)

#define CHECK_RET_NET(ret, err_msg) do { \
    if (ret < 0) { \
        printf("net failed: %s\n", err_msg); \
        return ret; \
    } \
} while (0)

#define CHECK_RET_ZERO(ret, err_msg) do { \
    if (ret != 0) { \
        printf("[failed] %s\n", err_msg); \
        return ret; \
    } \
} while (0)

// Get milliseconds relative to session start.
uint64_t rtmp_get_relative_ms(rtmp_ctx_t *ctx)
{
    return GetTickCount64() - ctx->self_tick_start;
}

// @return negative values on error.
int rtmp_send(rtmp_ctx_t *ctx, uint8_t *data, size_t len)
{
    int ret;
    if (len == 0) {
        return 0;
    }

    int try_count = 5;
    size_t sent_bytes = 0;
    do {
        ret = send(ctx->s, data, len, 0);
        if (ret == -1) {
            printf("failed to rtmp_send.\n");
            break;
        }
        sent_bytes += ret;
    } while (sent_bytes < len && --try_count > 0);

    return ret >= 0 ? 0 : -1;
}

// Read data from socket, with buffer employed.
// @return negative values on error.
int rtmp_read(rtmp_ctx_t *ctx, uint8_t *data, size_t num)
{
    int ret;
    size_t recv_bytes = 0;
    int retry_count = 5;
    do {
        ret = recv(ctx->s, data, num, 0);
        if (ret == 0) {
            printf("connection closed. recv return 0\n");
            break;
        } else if (ret == SOCKET_ERROR) {
            break;
        }
        recv_bytes += ret;
    } while (recv_bytes < num && --retry_count > 0);
    
    if (recv_bytes < num) {
        return -1;
    }
    return 0;
}

int rtmp_handshake(rtmp_ctx_t *ctx)
{
    int ret;

    // C0
    uint8_t version = 3;
    ret = send_data(&version, 1);
    CHECK_RET_NET(ret, "send C0");

    // Update ctx start time tick.
    ctx->self_tick_start = GetTickCount64();

    // C1
    ctx->c1 = malloc(C1_SIZE);
    uint8_t *c1 = ctx->c1;
    set_int32(c1, 0);
    set_int32(c1 + 4, 0);

    // Random bytes
    int rand_cur = rand();
    for (int i = 2; i != C1_SIZE/4; ++i) {
        int pos = i * 4;
        set_int32(c1 + pos, rand_cur++);
    }

    ret = send_data(c1, C1_SIZE);
    CHECK_RET_NET(ret, "send C1");

    // S0
    uint8_t s0;
    ret = recv_data(&s0, 1);
    CHECK_RET_NET(ret, "recv S0");

    // S1
    ctx->s1 = malloc(C1_SIZE);
    uint8_t *s1 = ctx->s1;
    ret = recv_data(s1, C1_SIZE);
    CHECK_RET_NET(ret, "recv S1");

    // Remote clock.
    ctx->remote_tick_start = get_int32(s1);
    printf("remote tick start in S1: %lld\n", ctx->remote_tick_start);

    // C2
    ctx->c2 = malloc(C1_SIZE);
    uint8_t *c2 = ctx->c2;
    set_int32(c2, ctx->remote_tick_start);
    set_int32(c2 + 4, rtmp_get_relative_ms(ctx));
    memcpy(c2 + 8, ctx->s1 + 8, C1_SIZE - 8);
    ret = send_data(c2, C1_SIZE);
    CHECK_RET_NET(ret, "send C2");

    // S2
    ctx->s2 = malloc(C1_SIZE);
    uint8_t *s2 = ctx->s2;
    ret = recv_data(s2, C1_SIZE);
    CHECK_RET_NET(ret, "recv S2");

    // Check S2 integrity.
    ret = memcmp(s2, c1, 4);
    if (ret != 0) {
        printf("timestamp in s2 doesn't equal to c1's\n");
    }
    ret = memcmp(s1, c2, 4);
    if (ret != 0) {
        printf("timestamp in c2 doesn't equal to s1's\n");
    }
    ret = memcmp(s2 + 8, c1 + 8, C1_SIZE - 8);
    if (ret != 0) {
        printf("S2 doesn't contain correct random echo\n");
        return -1;
    }

    printf("c1 recv timestamp: %u\n", get_int32(s2 + 4));

    return 0;
}

static void rtmp_reset_send_buf(rtmp_ctx_t *ctx)
{
    ctx->buf_send_pos = 0;
}

static rtmp_chunk_stream_t *create_chunk_stream(uint32_t csid, uint32_t msg_stream_id)
{
    rtmp_chunk_stream_t *cs = malloc(sizeof(*cs));
    memset(cs, 0, sizeof(*cs));
    cs->csid = csid;
    cs->msid = msg_stream_id;
    cs->last_valid = 0;

    // Create buffer for message.
    cs->msg_buf_capacity = 8 * 1024 * 1024;
    cs->msg_buf = malloc(cs->msg_buf_capacity);
    cs->msg_buf_pos = 0;

    return cs;
}

// Create necessary sending chunk streams.
// @return 0 on success.
static int rtmp_create_chunk_streams_local(rtmp_ctx_t *ctx)
{
    // Need to create 3 chunk streams.
    rtmp_chunk_stream_t *cs_chunk_control = create_chunk_stream(
        ctx->csid_chunk_protocol, 0);
    rtmp_chunk_stream_t *cs_netconn = create_chunk_stream(
        ctx->csid_netconnection, 0);
    rtmp_chunk_stream_t *cs_netstream = create_chunk_stream(
        ctx->csid_netstream, 1);

    ctx->send_chunk_streams[0] = cs_chunk_control;
    ctx->send_chunk_streams[1] = cs_netconn;
    ctx->send_chunk_streams[2] = cs_netstream;
    ctx->send_cs_count = 3;

    return 0;
}

static rtmp_chunk_stream_t *get_send_chunk_stream(rtmp_ctx_t *ctx, uint32_t csid)
{
    for (int i = 0; i != ctx->send_cs_count; ++i) {
        if (ctx->send_chunk_streams[i]->csid == csid) {
            return ctx->send_chunk_streams[i];
        }
    }
    return NULL;
}

static rtmp_chunk_stream_t *get_recv_chunk_stream(rtmp_ctx_t *ctx, uint32_t csid)
{
    for (int i = 0; i != ctx->recv_cs_count; ++i) {
        if (ctx->recv_chunk_streams[i]->csid == csid) {
            return ctx->recv_chunk_streams[i];
        }
    }
    return NULL;
}

static int rtmp_do_send_data(rtmp_ctx_t *ctx, uint8_t *data, size_t len)
{
    int ret = send(ctx->s, data, len, 0);
    if (ret == -1) {
        return -1;
    }
    if (ret != len) {
        return -1;
    }

    return 0;
}

static int rtmp_flush_send_buffer(rtmp_ctx_t *ctx)
{
    int ret = rtmp_do_send_data(ctx, ctx->buf_send, ctx->buf_send_pos);
    ctx->buf_send_pos = 0;
    return ret;
}

static int rtmp_send_chunk_head(rtmp_ctx_t *ctx, rtmp_chunk_stream_t *cs,
    size_t msg_len, uint8_t msg_type_id, uint32_t timestamp, int fmt)
{
    rtmp_reset_send_buf(ctx);

    // Form chunk header.
    uint8_t b;

    // chunk basic header
    b = (fmt << 6) | (cs->csid & 0x3F);
    ctx->buf_send[ctx->buf_send_pos++] = b;

    // chunk message header
    if (fmt == 0 || fmt == 1 || fmt == 2) {
        // TODO timestamp should be diff when fmt is 1 or 2.
        set_int24(ctx->buf_send + ctx->buf_send_pos, timestamp);
        ctx->buf_send_pos += 3;
    }

    if (fmt == 0 || fmt == 1) {
        set_int24(ctx->buf_send + ctx->buf_send_pos, msg_len);
        ctx->buf_send_pos += 3;
        set_int8(ctx->buf_send + ctx->buf_send_pos, msg_type_id);
        ctx->buf_send_pos += 1;
    }

    if (fmt == 0) {
        set_int32(ctx->buf_send + ctx->buf_send_pos, cs->msid);
        ctx->buf_send_pos += 4;
    }

    // Extended timestamp.
    // TODO

    return rtmp_flush_send_buffer(ctx);
}

static int rtmp_send_message(rtmp_ctx_t *ctx, rtmp_chunk_stream_t *cs, uint8_t *msg_data, 
    size_t msg_len, uint8_t msg_type_id, uint32_t timestamp)
{
    int ret;
    int first_chunk = 1;
    uint8_t *pos = msg_data;
    size_t to_send_len = msg_len;
    while (to_send_len > 0) {
        if (first_chunk) {
            rtmp_send_chunk_head(ctx, cs, msg_len, msg_type_id, timestamp, 0);
        } else {
            rtmp_send_chunk_head(ctx, cs, msg_len, msg_type_id, timestamp, 3);
        }

        size_t send_len = ctx->send_max_chunk_size < to_send_len ? ctx->send_max_chunk_size : to_send_len;
        ret = rtmp_do_send_data(ctx, pos, send_len);

        pos += send_len;
        to_send_len -= send_len;
        first_chunk = 0;
    }
    return ret;
}

static int rtmp_send_netconn_msg(rtmp_ctx_t *ctx, uint8_t *msg_data, size_t msg_len)
{
    rtmp_chunk_stream_t *s = get_send_chunk_stream(ctx, ctx->csid_netconnection);
    return rtmp_send_message(ctx, s, msg_data, msg_len, RTMP_MSG_TYPE_AMF0, 0);
}

// NetConnection connect
int rtmp_connect(rtmp_ctx_t *ctx)
{
    uint8_t payload[1024];
    int pos = 0;
    int endpos = sizeof(payload);
    int ret;

    ret = rtmp_create_chunk_streams_local(ctx);
    if (ret != 0) {
        printf("failed to create chunk streams on client side\n");
        return -1;
    }

    pos += amf0_write_string("connect", payload + pos, endpos - pos);
    pos += amf0_write_number(1, payload + pos, endpos - pos);

    pos += amf0_write_obj_start(payload + pos, endpos - pos);

    pos += amf0_write_obj_property_name("app", payload + pos, endpos - pos);
    pos += amf0_write_string(ctx->app, payload + pos, endpos - pos);
    pos += amf0_write_obj_property_name("flashVer", payload + pos, endpos - pos);
    pos += amf0_write_string("FMSc/1.0", payload + pos, endpos - pos);
    pos += amf0_write_obj_property_name("tcUrl", payload + pos, endpos - pos);
    pos += amf0_write_string(ctx->tc_url, payload + pos, endpos - pos);
    pos += amf0_write_obj_property_name("fpad", payload + pos, endpos - pos);
    pos += amf0_write_bool(0, payload + pos, endpos - pos);
    pos += amf0_write_obj_property_name("capacities", payload + pos, endpos - pos);
    pos += amf0_write_number(15, payload + pos, endpos - pos);
    pos += amf0_write_obj_property_name("audioCodecs", payload + pos, endpos - pos);
    pos += amf0_write_number(0x0FFF, payload + pos, endpos - pos);
    pos += amf0_write_obj_property_name("videoCodecs", payload + pos, endpos - pos);
    pos += amf0_write_number(0x00FF, payload + pos, endpos - pos);
    pos += amf0_write_obj_property_name("videoFunction", payload + pos, endpos - pos);
    pos += amf0_write_number(1, payload + pos, endpos - pos);

    pos += amf0_write_obj_end(payload + pos, endpos - pos);
    printf("NetConnection connect payload length: %d\n", pos);

    // Send in chunk.
    ret = rtmp_send_netconn_msg(ctx, payload, pos);
    CHECK_RET_NET(ret, "send 'connect'");

    return 0;
}

static int rtmp_create_stream(rtmp_ctx_t *ctx)
{
    uint8_t payload[1024];
    int pos = 0;
    int endpos = sizeof(payload);
    int ret;

    rtmp_chunk_stream_t *cs = get_send_chunk_stream(ctx, ctx->csid_netconnection);

    pos += amf0_write_string("createStream", payload + pos, endpos - pos);
    pos += amf0_write_number(2, payload + pos, endpos - pos);
    pos += amf0_write_null(payload + pos, endpos - pos);

    ret = rtmp_send_message(ctx, cs, payload, pos, RTMP_MSG_TYPE_AMF0, 0);
    CHECK_RET_NET(ret, "send 'createStream'");

    return 0;
}

static int rtmp_play(rtmp_ctx_t *ctx, const char *stream)
{
    uint8_t payload[1024];
    int pos = 0;
    int endpos = sizeof(payload);
    int ret;

    rtmp_chunk_stream_t *cs = get_send_chunk_stream(ctx, ctx->csid_netconnection);

    pos += amf0_write_string("play", payload + pos, endpos - pos);
    pos += amf0_write_number(3, payload + pos, endpos - pos);
    pos += amf0_write_null(payload + pos, endpos - pos);
    pos += amf0_write_string(stream, payload + pos, endpos - pos);
    pos += amf0_write_number(-2000, payload + pos, endpos - pos);

    ret = rtmp_send_message(ctx, cs, payload, pos, RTMP_MSG_TYPE_AMF0, 0);
    CHECK_RET_NET(ret, "send 'play'");

    return 0;
}

static int rtmp_send_window_ack_size(rtmp_ctx_t *ctx, uint32_t window_size)
{
    rtmp_chunk_stream_t *cs = get_send_chunk_stream(ctx, ctx->csid_chunk_protocol);

    char data[4];
    set_int32(data, window_size);
    return rtmp_send_message(ctx, cs, data, 4, 5, 0);
}

// Try to recv data, in nonblocking mode.
static int rtmp_recv_nonblocking(rtmp_ctx_t *ctx)
{
    int ret;
    if (ctx->buf_recv_pos == ctx->buf_recv_capacity) {
        printf("recv buffer full\n");
        return -1;
    }

    // Set socket to nonblocking.
    long mode = 1;
    int result = ioctlsocket(ctx->s, FIONBIO, &mode);
    if (result == SOCKET_ERROR) {
        printf("failed to set socket to nonblocking\n");
        return -1;
    }

    ret = recv(ctx->s, ctx->buf_recv + ctx->buf_recv_pos, ctx->buf_recv_capacity - ctx->buf_recv_pos, 0);

    // Return to blocking state.
    mode = 0;
    result = ioctlsocket(ctx->s, FIONBIO, &mode);
    if (result == SOCKET_ERROR) {
        printf("failed to set socket to blocking\n");
        return -1;
    }

    if (ret > 0) {
        printf("rtmp recv. bytes received: %d\n", ret);
        ctx->buf_recv_pos += ret;
        return 0;
    }

    return -1;
}

// csid == 2
static int rtmp_handle_chunk_protocol(rtmp_ctx_t *ctx, rtmp_chunk_stream_t *cs, uint8_t msg_type_id,
    uint8_t *msg, size_t len)
{
    printf("handle chunk protocol (2). msg type id: %u, len: %zu\n", msg_type_id, len);

    if (msg_type_id == 1) { // Set chunk size.
        if (len < 4) {
            printf("failed to parse Set Chunk Size\n");
            return -1;
        }
        uint32_t v = get_int32(msg);
        v = v & 0x7FFFFFFF;
        ctx->recv_max_chunk_size = v;
        printf("Set Chunk Size handled: %u\n", ctx->recv_max_chunk_size);
    } else if (msg_type_id == 5) { // Window Acknowledgement size.
        
    } else if (msg_type_id == 6) { // Set Peer Bandwidth.
        
    }

    return 0;
}

static int rtmp_handle_script_data(rtmp_ctx_t *ctx, rtmp_chunk_stream_t *cs, uint8_t *msg, size_t len)
{
    printf("  script data. len: %zu\n", len);
    return 0;
}

static int rtmp_handle_video_data(rtmp_ctx_t *ctx, rtmp_chunk_stream_t *cs, uint8_t *msg, size_t len)
{
    printf("  video data. len: %zu\n", len);
    return 0;
}

static int rtmp_handle_audio_data(rtmp_ctx_t *ctx, rtmp_chunk_stream_t *cs, uint8_t *msg, size_t len)
{
    printf("  audio data. len: %zu\n", len);
    return 0;
}

static int rtmp_handle_recv_message(rtmp_ctx_t *ctx, rtmp_chunk_stream_t *cs, uint8_t msg_type_id,
    uint8_t *msg, size_t len)
{
    int ret;
    char print_msg[1000];
    printf("handle recv message. csid: %u, msg type id: %u, len: %zu\n", 
        cs->csid, msg_type_id, len);

    if (msg_type_id == 18) {    // script data
        return rtmp_handle_script_data(ctx, cs, msg, len);
    } else if (msg_type_id == 9) {  // video
        return rtmp_handle_video_data(ctx, cs, msg, len);
    } else if (msg_type_id == 8) {  // audio
        return rtmp_handle_audio_data(ctx, cs, msg, len);
    }

    if (msg_type_id != 20) {
        printf("Only support AMF0(20), script(18), video(9), audio(8) currently\n");
        return -1;
    }

    amf0_t v = { 0 };
    size_t used_len = 0;
    while (used_len < len) {
        ret = amf0_parse(msg + used_len, len - used_len, &v);
        if (ret <= 0) {
            printf("failed to parse recv message\n");
            break;
        }

        used_len += ret;

        amf0_to_string(&v, print_msg, sizeof(print_msg));
        printf("  (got amf type: %d) %s\n", v.type, print_msg);
    }

    if (used_len < len) {
        printf("  not all bytes consumed. len: %zu, used: %d\n", len, ret);
        return -1;
    }

    return 0;
}

// Try to handle one chunk.
// @return negative if error or need more data; used bytes if handled.
static int rtmp_handle_chunk(rtmp_ctx_t *ctx, rtmp_chunk_stream_t *cs, uint8_t *data, size_t len)
{
    uint8_t b;
    int ret;
    uint8_t *pos = data;
    const uint8_t *pos_end = pos + len;

    uint32_t timestamp;
    uint32_t message_len;
    uint8_t msg_type_id;
    uint32_t msg_stream_id;

    // Parse chunk header.
    uint8_t fmt = *pos >> 6;
    pos++;

    // Not handle csid 0 and 1 for now.

    if (fmt == 0 || fmt == 1 || fmt == 2) {
        if (pos + 3 > pos_end) {
            return -1;
        }
        timestamp = get_int24(pos);
        pos += 3;

        cs->last_timestamp_value = timestamp;
    } else {
        timestamp = cs->last_timestamp_value;
    }

    if (fmt == 0 || fmt == 1) {
        if (pos + 4 > pos_end) {
            return -1;
        }
        message_len = get_int24(pos);
        pos += 3;
        msg_type_id = *pos++;

        cs->last_msg_length = message_len;
        cs->last_msg_type_id = msg_type_id;
    } else {
        message_len = cs->last_msg_length;
        msg_type_id = cs->last_msg_type_id;
    }

    if (fmt == 0) {
        if (pos + 4 > pos_end) {
            return -1;
        }
        msg_stream_id = get_int32(pos);
        pos += 4;

        cs->last_msg_stream_id = msg_stream_id;
    } else {
        msg_stream_id = cs->last_msg_stream_id;
    }

    // Extended timestamp.

    // Payload.
    uint8_t *msg_start = NULL;
    if (message_len < ctx->recv_max_chunk_size) {
        // Whole message.
        msg_start = pos;
        pos += message_len;
    } else {
        size_t this_data_len;
        if (ctx->recv_max_chunk_size + cs->msg_buf_pos < message_len) {
            this_data_len = ctx->recv_max_chunk_size;
        } else {
            this_data_len = message_len - cs->msg_buf_pos;
        }

        memcpy(cs->msg_buf, pos, this_data_len);
        cs->msg_buf_pos += this_data_len;
        pos += this_data_len;

        if (cs->msg_buf_pos == message_len) {
            rtmp_handle_recv_message(ctx, cs, msg_type_id, cs->msg_buf, cs->msg_buf_pos);
            cs->msg_buf_pos = 0;
        }
    }

    if (msg_start) {
        if (cs->csid == 2) {
            rtmp_handle_chunk_protocol(ctx, cs, msg_type_id, msg_start, message_len);
        } else {
            rtmp_handle_recv_message(ctx, cs, msg_type_id, msg_start, message_len);
        }
    }

    return pos - data;
}

// @return Number of chunks handled.
static int rtmp_handle(rtmp_ctx_t *ctx)
{
    uint8_t b;
    int ret;
    int handle_chunk_count = 0;

    while (ctx->buf_recv_pos > 0) {
        uint8_t *pos = ctx->buf_recv;
        size_t len = ctx->buf_recv_pos;

        b = *pos;
        uint8_t csid = b & 0x3F;

        int create = 0;
        rtmp_chunk_stream_t *cs = get_recv_chunk_stream(ctx, csid);
        if (NULL == cs) {
            cs = create_chunk_stream(csid, 0);
            ctx->recv_chunk_streams[ctx->recv_cs_count++] = cs;
            create = 1;
        }

        ret = rtmp_handle_chunk(ctx, cs, pos, len);
        if (ret < 0) {
            break;
        }
        handle_chunk_count++;

        size_t remain_len = len - ret;
        if (remain_len > 0) {
            memmove(ctx->buf_recv, ctx->buf_recv + len - remain_len, remain_len);
        }
        ctx->buf_recv_pos = remain_len;
    }

    return handle_chunk_count;
}

int rtmp_client_parse(rtmp_ctx_t *ctx)
{
    int ret;

    ret = rtmp_handshake(ctx);
    if (ret != 0) {
        printf("handshake failed.\n");
        return ret;
    }

    // connect
    ret = rtmp_connect(ctx);
    if (ret != 0) {
        printf("rtmp connect failed.\n");
        return ret;
    }

    Sleep(500); // Wait for server to send some commands.

    rtmp_recv_nonblocking(ctx);
    rtmp_handle(ctx);

    // Send window acknowledge size.
    rtmp_send_window_ack_size(ctx, 6000000);

    rtmp_create_stream(ctx);
    Sleep(100);
    rtmp_recv_nonblocking(ctx);
    rtmp_handle(ctx);

    rtmp_play(ctx, ctx->stream);

    for (int i = 0; i != 1000; ++i) {
        Sleep(10);
        rtmp_recv_nonblocking(ctx);
        rtmp_handle(ctx);
    }

    Sleep(1000);

    return 0;
}
