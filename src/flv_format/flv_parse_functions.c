#include "flv_parse_functions.h"

#include <stdlib.h>
#include <string.h>

#include "AMF.h"
#include "read_utils.h"
#include "decoder_config_record.h"

// Wrapper functions for flv_ctx_t
static uint32_t read_int8_flv(flv_ctx_t *ctx) { return read_int8(ctx->f); }
static uint32_t read_int16_flv(flv_ctx_t *ctx) { return read_int16(ctx->f); }
static uint32_t read_int24_flv(flv_ctx_t *ctx) { return read_int24(ctx->f); }
static uint32_t read_int32_flv(flv_ctx_t *ctx) { return read_int32(ctx->f); }
static uint64_t read_int48_flv(flv_ctx_t *ctx) { return read_int48(ctx->f); }
static uint64_t read_int64_flv(flv_ctx_t *ctx) { return read_int64(ctx->f); }
static int skip_bytes_flv(flv_ctx_t *ctx, int64_t bytes) { return skip_bytes(ctx->f, bytes); }
static int read_bytes_flv(flv_ctx_t *ctx, int64_t bytes, void *dst) { return read_bytes(ctx->f, bytes, dst); }

// Accelerated macros for reading ints
#define read8()    read_int8_flv(ctx)
#define read16()   read_int16_flv(ctx)
#define read24()   read_int24_flv(ctx)
#define read32()   read_int32_flv(ctx)
#define read48()   read_int48_flv(ctx)
#define read64()   read_int64_flv(ctx)

static void print_hex(uint8_t *data, size_t len, size_t prefix_white, int add_space)
{
    for (int i = 0; i != prefix_white; ++i) {
        printf(" ");
    }
    printf("(len: %lld) ", len);
    for (int i = 0; i != len; ++i) {
        if (!add_space && i > 0 && i % 4 == 0) {
            printf(" ");
        }
        printf("%02X%s", data[i], add_space ? " " : "");
    }
    printf("\n");
}

static int parse_flv_header(flv_ctx_t *ctx)
{
    char signature[4] = { 0 };
    read_bytes_flv(ctx, 3, signature);

    if (strcmp("FLV", signature) != 0) {
        printf("File signature is not FLV\n");
        return -1;
    }

    uint8_t version = read8();
    printf("FLV version: %u\n", version);

    uint8_t b = read8();
    uint8_t flag_audio = (b >> 2) & 0x1;
    uint8_t flag_video = b & 0x1;
    printf("flag audio: %u, flag video: %u\n", flag_audio, flag_video);

    uint32_t offset = read32();
    printf("header length: %u\n", offset);

    if (offset > 9) {
        skip_bytes_flv(ctx, offset - 9);
    }

    return 0;
}

static int parse_scriptdata(flv_ctx_t *ctx, uint8_t *data, size_t data_len)
{
    int ret;
    amf0_t amf_name;
    amf0_t amf_value;

    // Name.
    ret = amf0_parse(data, data_len, &amf_name);
    if (ret < 0) {
        printf("failed to parse script data name\n");
        return ret;
    }
    data += ret;
    data_len -= ret;

    printf("amf0 name type: %u\n", amf_name.type);
    if (amf_name.type != AMF0_STRING) {
        printf("name type incorrect!\n");
        return -1;
    }
    printf("scriptdata name: %s\n", amf_name.d.str);

    // Value.
    ret = amf0_parse(data, data_len, &amf_value);
    if (ret < 0) {
        printf("failed to parse script data value\n");
        return ret;
    }
    data += ret;
    data_len -= ret;

    if (amf_value.type != AMF0_ECMAARRAY) {
        printf("SCRIPTDATA value should be ecma array!\n");
        return -1;
    }
    printf("SCRIPTDATA properties:\n");
    for (int i = 0; i != amf_value.ecma_array_count; ++i) {
        char msg[40];
        amf0_to_string(amf_value.ecma_properties[i].value, msg, sizeof(msg));
        printf("  %s: %s\n", amf_value.ecma_properties[i].name, msg);
    }

    if (data_len != 0) {
        printf("not all bytes are used when parsing scriptdata. remain: %zu\n", data_len);
    }

    return 0;
}

static int parse_AVCVIDEOPACKET(flv_ctx_t *ctx, uint8_t *data, size_t data_len,
    uint8_t avc_packet_type)
{
    int ret;
    int total_data_len = data_len;

    if (avc_packet_type == 0) {
        // AVCDecoderConfigurationRecord
        uint8_t *sps;
        uint8_t *pps;
        size_t sps_len, pps_len;
        ret = avc_decoder_record_parse(data, data_len, &sps, &sps_len, &pps, &pps_len);
        if (ret != 0) {
            printf("failed to parse AVCDecoderConfigurationRecord in AVCVIDEOPACKET\n");
            return -1;
        }
        printf("got sps/pps:\n");
        print_hex(sps, sps_len, 2, 0);
        print_hex(pps, pps_len, 2, 0);
    } else if (avc_packet_type == 1) {
        // nalu/nalus
        int max_print = data_len;
        if (max_print > 20) {
            max_print = 20;
        }
        printf("  nalu start: \n");
        print_hex(data, max_print, 2, 0);
    }

    return total_data_len;
}

static int parse_videodata(flv_ctx_t *ctx, uint8_t *data, size_t data_len)
{
    int ret = 0;
    uint8_t b;
    uint8_t avc_packet_type;

    if (data_len < 5) {
        printf("video tag data len invalid: %zu\n", data_len);
        return -1;
    }

    // VideoTagHeader
    b = *data++;
    data_len--;

    uint8_t frame_type = b >> 4;
    uint8_t codec_id = b & 0xF;

    // AVC.
    if (codec_id == 7) {
        avc_packet_type = *data++;
            // 0: sequence header; 1: nalu; 2: end of nalu.
        data_len--;
        //printf("  AVC packet type: %u\n", avc_packet_type);

        uint32_t n = get_int24(data);
        data += 3;
        data_len -= 3;
        if (avc_packet_type == 1) {
            
        }
    }

    if (data_len <= 0) {
        printf("no enough data for VideoTagBody\n");
        return -1;
    }

    // VideoTagBody
    if (frame_type == 5) {
        data++;
        data_len--;
    } else {
        if (codec_id == 7) {
            // AVCVIDEOPACKET
            ret = parse_AVCVIDEOPACKET(ctx, data, data_len, avc_packet_type);
            if (ret < 0) {
                printf("failed to parse AVCVIDEOPACKET\n");
                return ret;
            }
            data += ret;
            data_len -= ret;
        }
    }

    return 0;
}

static int parse_AACAUDIODATA(flv_ctx_t *ctx, uint8_t *data, size_t data_len,
    uint8_t aac_packet_type)
{
    if (aac_packet_type == 0) {
        // AudioSpecificConfig
        printf("  AudioSpecificConfig. Not parsed now.\n");
    } else if (aac_packet_type == 1) {
        // raw aac frame.
        int max_print = data_len;
        if (max_print > 20) {
            max_print = 20;
        }
        printf("  aac raw start: \n");
        print_hex(data, max_print, 2, 0);
    }

    return 0;
}

static int parse_audiodata(flv_ctx_t *ctx, uint8_t *data, size_t data_len)
{
    int ret;
    uint8_t b;
    uint8_t aac_packet_type;

    if (data_len < 2) {
        printf("invalid audio data len: %zu\n", data_len);
        return -1;
    }

    // AudioTagHeader
    b = *data++;
    data_len--;

    uint8_t audio_format = b >> 4;
    uint8_t audio_rate = (b >> 2) & 0x3; // 0: 5.5k; 1: 11k; 2: 22k; 3: 44k. in Hz.
    uint8_t audio_size = (b >> 1) & 0x1; // 0: 8-bit sample; 1: 16-bit sample.
    uint8_t audio_type = b & 0x1;        // 0: mono; 1: stereo

    if (audio_format == 10) {
        // AAC.
        aac_packet_type = *data++;
        data_len--;
    }

    // AUDIODATA
    // AudioTagBody
    if (audio_format == 10) {
        // AACAUDIODATA
        ret = parse_AACAUDIODATA(ctx, data, data_len, aac_packet_type);
        if (ret < 0) {
            printf("failed to parse AACAUDIODATA\n");
            return ret;
        }
        data += ret;
        data_len -= ret;
    } else {
        // others. not process.
    }

    return 0;
}

static int parse_next_tag(flv_ctx_t *ctx)
{
    int ret = 0;
    uint32_t previous_tag_size = read32();

    uint8_t b = read8();
    uint8_t filter = (b >> 5) & 0x1;
    uint8_t tag_type = b & 0x1F;
    uint32_t data_size = read24();
    uint32_t ts = read24();
    uint8_t ts_extend = read8();
    uint32_t stream_id = read24();

    printf("tag type: %u, data size:%u (previous tag size: %u)\n", 
        tag_type, data_size, previous_tag_size);

    int64_t next_tag_pos = _ftelli64(ctx->f) + data_size;

    if (filter == 1) {
        printf("not support FILTER\n");
        return -1;
    }

    // Read bytes into memory first.
    uint8_t *tag_data = malloc(data_size);
    read_bytes_flv(ctx, data_size, tag_data);

    if (tag_type == 18) {
        // script data.
        printf("to process scriptdata.\n");

        // scriptdata
        // ScritpTagBody (AMF0)
        ret = parse_scriptdata(ctx, tag_data, data_size);
    } else if (tag_type == 8) {
        // audio.
        ret = parse_audiodata(ctx, tag_data, data_size);
    } else if (tag_type == 9) {
        // video.
        ret = parse_videodata(ctx, tag_data, data_size);
    } else {
        printf("unknown tag type: %u\n", tag_type);
        return -1;
    }

    if (ret != 0) {
        return ret;
    }

    int64_t current_pos = _ftelli64(ctx->f);
    if (current_pos != next_tag_pos) {
        printf("remain bytes not used before next tag: %lld\n", next_tag_pos - current_pos);
        _fseeki64(ctx->f, next_tag_pos, SEEK_SET);
    }

    ctx->tag_count++;

    return 0;
}

int parse_flv_file(const char *filename, flv_ctx_t *ctx)
{
    printf("start parse.\n");

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
    int64_t file_size;
    _fseeki64(ctx->f, 0, SEEK_END);
    file_size = _ftelli64(ctx->f);
    _fseeki64(ctx->f, 0, SEEK_SET);
    printf("file size: %llu\n", file_size);

    ret = parse_flv_header(ctx);
    if (ret != 0) {
        printf("failed to parse flv header.\n");
        return ret;
    }

    while (_ftelli64(ctx->f) + 4 < file_size && ctx->tag_count < 40) {
        ret = parse_next_tag(ctx);
        if (ret != 0) {
            printf("parse tag failed.\n");
            return ret;
        }
    }

    printf("end parse\n");
    return 0;
}
