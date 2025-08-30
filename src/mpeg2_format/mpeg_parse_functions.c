#include <assert.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "mpeg_defs.h"
#include "read_utils.h"

// Wrapper functions for mpeg_ctx_t
static uint32_t read_int8_mpeg(mpeg_ctx_t *ctx) { return read_int8(ctx->f); }
static uint32_t read_int16_mpeg(mpeg_ctx_t *ctx) { return read_int16(ctx->f); }
static uint32_t read_int24_mpeg(mpeg_ctx_t *ctx) { return read_int24(ctx->f); }
static uint32_t read_int32_mpeg(mpeg_ctx_t *ctx) { return read_int32(ctx->f); }
static uint64_t read_int48_mpeg(mpeg_ctx_t *ctx) { return read_int48(ctx->f); }
static uint64_t read_int64_mpeg(mpeg_ctx_t *ctx) { return read_int64(ctx->f); }
static int skip_bytes_mpeg(mpeg_ctx_t *ctx, int64_t bytes) { return skip_bytes(ctx->f, bytes); }
static int read_bytes_mpeg(mpeg_ctx_t *ctx, int64_t bytes, void *dst) { return read_bytes(ctx->f, bytes, dst); }
static void back_bytes_mpeg(mpeg_ctx_t *ctx, int64_t bytes) { _fseeki64(ctx->f, -bytes, SEEK_CUR); }

// Accelerated macros for reading ints
#define read8()    read_int8_mpeg(ctx)
#define read16()   read_int16_mpeg(ctx)
#define read24()   read_int24_mpeg(ctx)
#define read32()   read_int32_mpeg(ctx)
#define read48()   read_int48_mpeg(ctx)
#define read64()   read_int64_mpeg(ctx)

static const uint8_t pes_prefix_code[] = { 0x00, 0x00, 0x01 };
static uint8_t buffer[4 * 1024 * 1024];
static const int ts_packet_size = 188;

static int parse_ts_file(mpeg_ctx_t *ctx);
static int parse_ps_file(mpeg_ctx_t *ctx);

int parse_mpeg_file(const char *filename, mpeg_ctx_t *ctx, int is_ts)
{
    int ret;

    if (ctx->f != NULL) {
        printf("file stream is not null\n");
        return -1;
    }

    ctx->f = fopen(filename, "rb");
    if (!ctx->f) {
        printf("failed to open file: %s\n", filename);
        return -1;
    }

    // Get file size.
    _fseeki64(ctx->f, 0, SEEK_END);
    ctx->file_size = _ftelli64(ctx->f);
    _fseeki64(ctx->f, 0, SEEK_SET);

    if (is_ts) {
        return parse_ts_file(ctx);
    } else {
        return parse_ps_file(ctx);
    }

    return 0;
}

static mpeg_stream_t *get_stream_by_type(mpeg_ctx_t *ctx, uint8_t stream_type)
{
    for (int i = 0; i != ctx->stream_count; ++i) {
        if (ctx->streams[i] && ctx->streams[i]->stream_type == stream_type) {
            return ctx->streams[i];
        }
    }
    return NULL;
}

static mpeg_stream_t *get_stream_by_pid(mpeg_ctx_t *ctx, uint16_t pid)
{
    for (int i = 0; i != ctx->stream_count; ++i) {
        if (ctx->streams[i] && ctx->streams[i]->pid == pid) {
            return ctx->streams[i];
        }
    }
    return NULL;
}

static mpeg_stream_t *get_stream_by_streamid(mpeg_ctx_t *ctx, uint8_t stream_id)
{
    for (int i = 0; i != ctx->stream_count; ++i) {
        if (ctx->streams[i] && ctx->streams[i]->stream_id == stream_id) {
            return ctx->streams[i];
        }
    }
    return NULL;
}

// @return 0 on success or no error.
static int add_if_stream_not_exist(mpeg_ctx_t *ctx, uint8_t stream_type, uint16_t pid)
{
    if (NULL == get_stream_by_type(ctx, stream_type)) {
        mpeg_stream_t *stream = malloc(sizeof(mpeg_stream_t));
        memset(stream, 0, sizeof(mpeg_stream_t));

        if (MPEG_ST_AVC == stream_type) {
            stream->is_avc = 1;
            strcpy(stream->stream_str, "avc");
        } else if (MPEG_ST_HEVC == stream_type) {
            stream->is_hevc = 1;
            strcpy(stream->stream_str, "hevc");
        } else if (MPEG_ST_AAC == stream_type) {
            stream->is_aac = 1;
            strcpy(stream->stream_str, "aac");
        } else {
            printf("unsupported stream type: %u\n", stream_type);
            free(stream);
            return -1;
        }

        stream->pid = pid;
        stream->stream_type = stream_type;

        // Open debug file.
        char debug_file[100];
        sprintf(debug_file, "%s.pes", stream->stream_str);
        stream->debug_pes_f = fopen(debug_file, "wb");
        sprintf(debug_file, "%s.es", stream->stream_str);
        stream->debug_es_f = fopen(debug_file, "wb");

        // pes cache.
        stream->pes_capacity = 4 * 1024 * 1024;
        stream->pes_data = malloc(stream->pes_capacity);
        stream->pes_length = 0;

        ctx->streams[ctx->stream_count++] = stream;

        printf("  stream added. pid: 0x%x, stream_type: 0x%x, str: %s\n",
            pid, stream_type, stream->stream_str);
    }

    return 0;
}

static void add_stream_with_stream_id(mpeg_ctx_t *ctx, uint8_t stream_id)
{
    if (NULL == get_stream_by_streamid(ctx, stream_id)) {
        mpeg_stream_t *stream = malloc(sizeof(mpeg_stream_t));
        memset(stream, 0, sizeof(mpeg_stream_t));

        stream->stream_id = stream_id;
        sprintf(stream->stream_str, "0x%x", stream_id);

        // Open debug file.
        char debug_file[100];
        sprintf(debug_file, "%s.pes", stream->stream_str);
        stream->debug_pes_f = fopen(debug_file, "wb");
        sprintf(debug_file, "%s.es", stream->stream_str);
        stream->debug_es_f = fopen(debug_file, "wb");

        // pes cache.
        stream->pes_capacity = 4 * 1024 * 1024;
        stream->pes_data = malloc(stream->pes_capacity);
        stream->pes_length = 0;

        ctx->streams[ctx->stream_count++] = stream;

        printf("  stream added. stream_id: 0x%x, str: %s\n",
            stream_id, stream->stream_str);
    }
}

static int process_ts_pat(mpeg_ctx_t *ctx, uint8_t *buffer, uint8_t length)
{
    int pos = 0;
    uint8_t b;
    uint8_t pointer_field = buffer[pos++];
    if (pointer_field > 0) {
        printf("pointer_field > 0: %hhu\n", pointer_field);
        pos += pointer_field;
    }

    uint8_t table_id = buffer[pos++];
    b = buffer[pos++];
    uint8_t section_syntax_indicator = b >> 7;
    uint16_t section_length = ((b & 0x0F) << 8) + buffer[pos++];
    uint32_t end_pos = section_length + pos;
    uint16_t transport_stream_id = get_int16(buffer + pos);
    pos += 2;
    uint8_t version_number = (buffer[pos] >> 1) & 0x1F;
    uint8_t current_next_indicator = buffer[pos] & 0x1;
    pos++;

    uint8_t section_number = buffer[pos++];
    uint8_t last_section_number = buffer[pos++];

    uint32_t N = (end_pos - 4 - pos) / 4;

    // We assume only 1 program is present.
    for (int i = 0; i != N; ++i) {
        uint16_t program_number = get_int16(buffer + pos);
        pos += 2;
        if (0 == program_number) {
            uint16_t network_pid = get_int16(buffer + pos) & 0x1FFF;
            printf("got network_pid: 0x%x\n", (int)network_pid);
        } else {
            uint16_t program_map_pid = get_int16(buffer + pos) & 0x1FFF;
            ctx->program_map_pid = program_map_pid;
            printf("got program_map_pid: 0x%x\n", (int)program_map_pid);
        }
        pos += 2;
    }

    uint32_t crc = get_int32(buffer + pos);
    pos += 4;

    if (pos != end_pos) {
        printf("invalid ts PAT! pos: %u, end_pos: %u\n", pos, end_pos);
    }

    return 0;
}

static int process_ts_pmt(mpeg_ctx_t *ctx, uint8_t *buffer, uint8_t length)
{
    int ret;
    int pos = 0;
    uint8_t b;
    uint16_t word;
    uint8_t pointer_field = buffer[pos++];
    if (pointer_field > 0) {
        printf("pointer_field > 0: %hhu\n", pointer_field);
        pos += pointer_field;
    }

    uint8_t table_id = buffer[pos++];
    b = buffer[pos++];
    uint8_t section_syntax_indicator = b >> 7;
    uint16_t section_length = ((b & 0x0F) << 8) + buffer[pos++];
    uint32_t end_pos = section_length + pos;

    uint16_t program_number = get_int16(buffer + pos);
    pos += 2;

    b = buffer[pos++];
    uint8_t version_number = (b >> 1) & 0x1F;
    uint8_t current_next_indicator = b & 0x1;

    uint8_t section_number = buffer[pos++];
    uint8_t last_section_number = buffer[pos++];
    uint16_t PCR_pid = get_int16(buffer + pos) & 0x1FFF;
    pos += 2;
    uint16_t program_info_length = get_int16(buffer + pos) & 0xFFF;
    pos += 2;

    // skip program info.
    pos += program_info_length;

    // Read stream mapping.
    while (pos + 5 <= end_pos - 4) {
        uint8_t stream_type = buffer[pos++];
        uint16_t es_pid = get_int16(buffer + pos) & 0x1FFF;
        pos += 2;
        uint16_t es_info_len = get_int16(buffer + pos) & 0xFFF;
        pos += 2;
        pos += es_info_len;

        ret = add_if_stream_not_exist(ctx, stream_type, es_pid);
        if (ret != 0) {
            printf("  failed to add stream!\n");
        }

        printf("  stream type: 0x%x, es_pid: 0x%x, es_info_len: %u\n",
            (uint32_t)stream_type, (uint32_t)es_pid, (uint32_t)es_info_len);
    }

    uint32_t crc = get_int32(buffer + pos);
    pos += 4;

    if (pos != end_pos) {
        printf("pos != end_pos in process_ts_pmt. pos: %u, end_pos: %u\n",
            (uint32_t)pos, (uint32_t)end_pos);
    }

    return 0;
}

// Parse one pes.
// @pre PES data is read fully.
static int process_one_pes(mpeg_stream_t *stream)
{
    int pos = 0;
    int end_pos = pos + stream->pes_length;
    uint8_t *buf = stream->pes_data;
    uint8_t b;
    uint16_t word;

    if (stream->pes_length < 9) {
        printf("pes length is too small: %u\n", stream->pes_length);
        return -1;
    }

    // prefix code.
    if (!(buf[0] == 0 && buf[1] == 0 && buf[2] == 1)) {
        printf("prefix code is not 00 00 01\n");
        return -1;
    }
    uint8_t stream_id = buf[3];
    uint16_t pes_packet_length = get_int16(buf + 4);
    pos = 6;

    b = buf[pos++];
    uint8_t pes_scrambling_control = b >> 4 & 0x3;
    uint8_t pes_priority = (b >> 3) & 0x1;
    uint8_t data_align_indicator = (b >> 2) & 0x1;
    uint8_t copyright = (b >> 1) & 0x1;
    uint8_t original_or_copy = b & 0x1;

    b = buf[pos++];
    uint8_t pts_dts_flag = b >> 6;
    uint8_t ESCR_flag = b >> 5 & 0x1;
    uint8_t ES_rate_flag = b >> 4 & 0x1;
    uint8_t DSM_trick_mode_flag = b >> 3 & 0x1;
    uint8_t additional_copy_info_flag = b >> 2 & 0x1;
    uint8_t pes_crc_flag = b >> 1 & 0x1;
    uint8_t pes_extension_flag = b & 0x1;

    uint8_t pes_header_length = buf[pos++];

    // Only print pts/dts now.
    uint8_t *pts_start = buf + pos;

    if (stream_id == 0xbe) {
        printf("Got padding stream. pes_header_length: %u\n", pes_header_length);
        return 0;
    }

    if (pts_dts_flag == 0x2) {
        uint64_t pts = 0;
        pts |= ((uint64_t)pts_start[0] << 4 & 0xF0) << 24;
        word = get_int16(pts_start + 1);
        pts |= word >> 1 << 15;
        word = get_int16(pts_start + 3);
        pts |= word >> 1;
        printf("  only pts: %llu\n", pts);
    } else if (pts_dts_flag == 0x3) {
        uint64_t pts = 0;
        uint64_t dts = 0;
        pts |= ((uint64_t)pts_start[0] << 4 & 0xF0) << 24;
        word = get_int16(pts_start + 1);
        pts |= word >> 1 << 15;
        word = get_int16(pts_start + 3);
        pts |= word >> 1;

        uint8_t *dts_start = pts_start + 5;
        dts |= ((uint64_t)dts_start[0] << 4 & 0xF0) << 24;
        word = get_int16(dts_start + 1);
        dts |= word >> 1 << 15;
        word = get_int16(dts_start + 3);
        dts |= word >> 1;

        printf("  pts: %llu, dts: %llu\n", pts, dts);
    } else {
        
    }

    // skip pes header for now.
    pos += pes_header_length;

    if (pos >= end_pos) {
        printf("invalid length of pes!\n");
        return -1;
    }

    if (stream->debug_es_f) {
        fwrite(buf + pos, end_pos - pos, 1, stream->debug_es_f);
    }

    stream->pes_count++;

    return 0;
}

static int process_ts_media_payload(mpeg_ctx_t *ctx, mpeg_stream_t *stream, uint8_t *buffer, 
    uint8_t length, uint8_t pusi)
{
    int ret;
    if (stream->debug_pes_f) {
        fwrite(buffer, length, 1, stream->debug_pes_f);
    }

    if (pusi) {
        // Output last pes.
        if (stream->pes_length > 0) {
            ret = process_one_pes(stream);
            if (ret != 0) {
                printf("failed to process PES!\n");
            }
        }
        stream->pes_length = 0;
    }

    if (length + stream->pes_length > stream->pes_capacity) {
        printf("pes data buffer overflow! Clear now.\n");
        stream->pes_length = 0;
    }

    memcpy(stream->pes_data + stream->pes_length, buffer, length);
    stream->pes_length += length;

    return 0;
}

static int process_ts_payload(mpeg_ctx_t *ctx, uint8_t *buffer, uint8_t length,
    uint16_t pid, uint8_t pusi)
{
    int ret;

    if (pid == 0) {
        printf("  PAT encountered\n");
        ret = process_ts_pat(ctx, buffer, length);
    } else if (ctx->program_map_pid == pid) {
        ret = process_ts_pmt(ctx, buffer, length);
    } else {
        // As media packet.
        mpeg_stream_t *stream = get_stream_by_pid(ctx, pid);
        if (stream == NULL) {
            printf("not media packet. pid: 0x%x\n", pid);
            return -1;
        }

        ret = process_ts_media_payload(ctx, stream, buffer, length, pusi);

        printf("got packet of media: %s, stream type: %u\n", stream->stream_str, stream->stream_type);
    }

    return 0;
}

// Assume buffer has length of 188
static int parse_ts_packet(mpeg_ctx_t *ctx, uint8_t *buffer)
{
    int ret;
    int pos = 0;

    // Check sync byte.
    if (buffer[pos] != 0x47) {
        printf("packet sync_byte is not 0x47\n");
        return -1;
    }
    ++pos;

    uint8_t transport_error_indicator = buffer[pos] >> 7;
    uint8_t payload_unit_start_indicator = buffer[pos] >> 6 & 0x1;
    uint8_t transport_priority = buffer[pos] >> 5 & 0x1;
    uint16_t pid = ((uint32_t)buffer[pos] << 8 & 0x1F00) + buffer[pos + 1];
    pos += 2;

    uint8_t transport_scrambling_control = buffer[pos] >> 6;
    uint8_t adaptation_field_control = buffer[pos] >> 4 & 0x3;
    uint8_t continuity_counter = buffer[pos] & 0xF;
    pos++;

    ctx->ts_packet_count++;

    if (adaptation_field_control == 0x2 || adaptation_field_control == 0x3) {
        uint8_t adaptation_field_length = buffer[pos];
        pos++;
        pos += adaptation_field_length;
    }
    if (adaptation_field_control == 0x1 || adaptation_field_control == 0x3) {
        uint8_t payload_length = 188 - pos;

        ret = process_ts_payload(ctx, buffer + pos, payload_length, pid, payload_unit_start_indicator);
    }

    printf("PID: 0x%x, ts payload length: %u\n", (uint32_t)pid, 188 - pos);

    return 0;
}

static int parse_ts_file(mpeg_ctx_t *ctx)
{
    int ret;
    printf("start parsing TS\n");

    for (;;) {
        ret = fread(buffer, ts_packet_size, 1, ctx->f);
        if (ret != 1) {
            printf("failed to read packet out\n");
            break;
        }

        ret = parse_ts_packet(ctx, buffer);
        if (ret != 0) {
            printf("process ts packet failed. packet index: %d\n", ctx->ts_packet_count);
            break;
        }
    }

    return 0;
}

static int parse_ps_pack_header(mpeg_ctx_t *ctx)
{
    uint8_t b;
    uint32_t start_code = read32();
    if (start_code != 0x000001ba) {
        printf("ps pack header prefix code invalid!\n");
        return -1;
    }

    skip_bytes_mpeg(ctx, 9);

    b = read8();
    uint8_t stuffing_len = b & 0x7;
    skip_bytes_mpeg(ctx, stuffing_len);

    uint32_t system_header_prefix = read32();
    if (system_header_prefix != 0x000001bb) {
        back_bytes_mpeg(ctx, 4);
        return 0;
    }

    uint16_t header_length = read16();
    skip_bytes_mpeg(ctx, header_length);

    return 0;
}

static int parse_ps_pes(mpeg_ctx_t *ctx, uint8_t *data, uint64_t content_len)
{
    uint8_t *end = data + content_len;
    uint8_t *pes = data;
    uint8_t *pes_end;
    uint8_t *pos = data + 1;

    int ret;

    while (pes + 5 < end) {
        // Ensure this is a good pes.
        if (!(pes[0] == 0x0 && pes[1] == 0x0 && pes[2] == 0x1)) {
            printf("Invalid pes, prefix code is not satisfied.\n");
            return -1;
        }

        // Find current pes end.
        uint16_t pes_len = get_int16(pes + 4);
        if (pes_len + pes + 4 > end) {
            printf("Invalid pes, length exceeds end. length: %u\n", pes_len);
            return -1;
        }
        pes_end = pes + pes_len + 6;

        if (pes_len < 1) {
            printf("invalid ps pes, length: %u\n", pes_len);
            return -1;
        }
        assert(pes_len >= 1);

        // Get stream_id.
        uint8_t stream_id = pes[3];
        printf("stream_id: 0x%x\n", stream_id);

        // Get or add stream.
        add_stream_with_stream_id(ctx, stream_id);
        mpeg_stream_t *stream = get_stream_by_streamid(ctx, stream_id);

        // Handle pes.
        stream->pes_length = pes_end - pes;
        memcpy(stream->pes_data, pes, stream->pes_length);
        ret = process_one_pes(stream);
        if (ret != 0) {
            printf("failed to process one pes in PS\n");
            return -1;
        }

        // Set next pes start.
        pes = pes_end;
    }
    return 0;
}

static int parse_ps_file(mpeg_ctx_t *ctx)
{
    int ret;
    printf("start parsing PS\n");

    int count = 0;
    while (_ftelli64(ctx->f) < ctx->file_size - 4) {
        count++;

        // Read until pack_header.
        while (_ftelli64(ctx->f) < ctx->file_size - 4) {
            uint32_t pack_start_code = read32();
            if (pack_start_code != 0x000001ba) {
                back_bytes_mpeg(ctx, 3);
            } else {
                printf("got pack_header. pos: %llu\n", _ftelli64(ctx->f));
                break;
            }
        }
        if (_ftelli64(ctx->f) >= ctx->file_size - 4) {
            printf("file end reached 1.\n");
            break;
        }
        back_bytes_mpeg(ctx, 4);

        ret = parse_ps_pack_header(ctx);
        if (ret != 0) {
            printf("failed to parse PS pack header\n");
            break;
        }

        // Get next pack pos.
        uint64_t pos_after_pack_header = _ftelli64(ctx->f);
        while (_ftelli64(ctx->f) < ctx->file_size - 4) {
            uint32_t pack_start_code = read32();
            if (pack_start_code != 0x000001ba) {
                back_bytes_mpeg(ctx, 3);
            } else {
                //printf("got pack_header 2. pos: %llu\n", _ftelli64(ctx->f));
                break;
            }
        }
        if (_ftelli64(ctx->f) >= ctx->file_size - 4) {
            printf("file end reached 2.\n");
            break;
        }
        back_bytes_mpeg(ctx, 4);
        uint64_t pos_next_pack_header = _ftelli64(ctx->f);
        _fseeki64(ctx->f, pos_after_pack_header, SEEK_SET);

        uint64_t pack_content_len = pos_next_pack_header - pos_after_pack_header;
        ret = fread(buffer, pack_content_len, 1, ctx->f);
        if (ret != 1) {
            printf("failed to read %llu of pack content\n", pack_content_len);
            break;
        }

        ret = parse_ps_pes(ctx, buffer, pack_content_len);
        if (ret != 0) {
            printf("failed to parse PS PES\n");
            break;
        }

        if (count % 100 == 0) {
            // print all created streams.
            printf("all streams: \n");
            mpeg_stream_t *stream;
            for (int i = 0; i != ctx->stream_count; ++i) {
                printf("  stream %d, stream_id: 0x%x, pes count: %llu\n", 
                    i, ctx->streams[i]->stream_id, ctx->streams[i]->pes_count);
            }
            printf("");
        }
    }

    printf("end parsing PS\n");

    return 0;
}