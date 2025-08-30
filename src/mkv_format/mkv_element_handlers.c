#include <stdlib.h>
#include <string.h>

#include "mkv_element_handlers.h"
#include "mkv_internal_func.h"
#include "decoder_config_record.h"

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

int ele_track_entry(mkv_ctx_t *ctx, void *p, size_t data_len)
{
    // Create a new track.
    mkv_track_t *track = malloc(sizeof(mkv_track_t));
    memset(track, 0, sizeof(mkv_track_t));

    // Set some defaults.
    track->ts_scale = 1.0;

    if (ctx->track_count >= sizeof(ctx->tracks) / sizeof(void *)) {
        printf("exceed largest track number support!\n");
        return -1;
    }

    ctx->tracks[ctx->track_count++] = track;
    ctx->cur_track = track;

    return 0;
}

int ele_track_number(mkv_ctx_t *ctx, void *p, size_t data_len)
{
    mkv_track_t *track = ctx->cur_track;
    track->id = *(uint64_t *)p;

    return 0;
}

int ele_track_codecid(mkv_ctx_t *ctx, void *p, size_t data_len)
{
    mkv_track_t *track = ctx->cur_track;
    strncpy(track->codec_str, p, sizeof(track->codec_str) - 1);

    return 0;
}

int ele_track_type(mkv_ctx_t *ctx, void *p, size_t data_len)
{
    mkv_track_t *track = ctx->cur_track;

    uint64_t type = *(uint64_t *)p;
    if (type == 1) {
        track->is_video = 1;
    } else if (type == 2) {
        track->is_audio = 1;
    } else if (type == 17) {
        track->is_subtitle = 17;
    } else {
        printf("unhandled track type: %llu\n", type);
    }

    return 0;
}

int ele_segment(mkv_ctx_t *ctx, void *p, size_t data_len)
{
    // Record segment start position. Element ID and Element Data Size are excluded already.
    ctx->segment_start = _ftelli64(ctx->f);

    return 0;
}

int ele_segment_uuid(mkv_ctx_t *ctx, void *p, size_t data_len)
{
    uint8_t *binary = p;
    print_hex(binary, data_len, ctx->depth * 2, 0);
    return 0;
}

int ele_timestamp_scale(mkv_ctx_t *ctx, void *p, size_t data_len)
{
    ctx->ts_scale = *(uint64_t *)p;
    return 0;
}

int ele_track_codec_private(mkv_ctx_t *ctx, void *p, size_t data_len)
{
    int ret;
    uint8_t *binary = p;
    mkv_track_t *track = ctx->cur_track;
    if (track->is_video && strcmp(track->codec_str, "V_MPEG4/ISO/AVC") == 0) {
        uint8_t *sps;
        uint8_t *pps;
        size_t sps_len;
        size_t pps_len;

        ret = avc_decoder_record_parse(binary, data_len, &sps, &sps_len, &pps, &pps_len);
        if (ret != 0) {
            printf("failed to parse SPS/PPS from AVC\n");
        } else {
            printf("%sSPS/PPS parsed from AAC:\n", get_depth_space(ctx->depth));
            print_hex(sps, sps_len, ctx->depth * 2 + 2, 0);
            print_hex(pps, pps_len, ctx->depth * 2 + 2, 0);
        }
    }

    //printf("%s(if avc, codec private is avcC)\n", get_depth_space(ctx->depth));
    print_hex(binary, data_len, ctx->depth * 2, 0);
    return 0;
}

int ele_cluster(mkv_ctx_t *ctx, void *p, size_t data_len)
{
    //if (ctx->cluster_count >= 3) {
    //    printf("only process %d cluster. abort.\n", ctx->cluster_count);
    //    return -1;
    //}

    // Extentd cluster array.
    if (ctx->cluster_capacity == ctx->cluster_count) {
        ctx->cluster_capacity = ctx->cluster_capacity * 2 + 1;
        if (ctx->clusters) {
            ctx->clusters = realloc(ctx->clusters, sizeof(mkv_cluster_t) * ctx->cluster_capacity);
        } else {
            ctx->clusters = malloc(sizeof(mkv_cluster_t) * ctx->cluster_capacity);
        }

        memset(ctx->clusters + ctx->cluster_count, 0,
            sizeof(mkv_cluster_t) * (ctx->cluster_capacity - ctx->cluster_count));
    }

    ctx->cluster_count++;
    ctx->cur_cluster = ctx->clusters + ctx->cluster_count - 1;

    // Calculate file position.
    ctx->cur_cluster->content_abs_pos = _ftelli64(ctx->f);

    return 0;
}

int ele_cluster_timestamp(mkv_ctx_t *ctx, void *p, size_t data_len)
{
    ctx->cur_cluster->timestamp = *(uint64_t *)p;
    return 0;
}

int ele_simple_block(mkv_ctx_t *ctx, void *p, size_t data_len)
{
    int ret;
    uint8_t *data = p;
    uint64_t track_number;
    uint8_t used_len;
    ret = get_VINT(data, data_len, &track_number, &used_len);
    if (ret != 0) {
        printf("failed to parse VINT in simple block\n");
        return ret;
    }
    data += used_len;
    data_len -= used_len;

    mkv_track_t *track = mkv_get_track_by_id(ctx, track_number);
    if (track == NULL) {
        printf("track not found: %llu\n", track_number);
        return -1;
    }

    if (data_len < 3) {
        printf("no more buffer data!\n");
        return -1;
    }

    int16_t timestamp_block = get_int16(data);
    data += 2;
    uint8_t b = *data;
    data++;
    data_len -= 3;
    if ((b & 0x70) != 0x00) {
        printf("Invalid Rsvrd byte: %u\n", b);
        return -1;
    }
    uint8_t key_frame = b >> 7;
    uint8_t invisible = (b >> 3) & 0x1;
    uint8_t lacing = (b >> 1) & 0x3;
    uint8_t discardable = b & 0x1;

    if (track_number == 1)      // for debug
    printf("%strack: %llu, timestamp: %d (%.2lf), whether key: %u, lacing: %u\n", 
        get_depth_space(ctx->depth),
        track_number, timestamp_block, 
        (ctx->cur_cluster->timestamp + timestamp_block * track->ts_scale) / 1000000000.0 * ctx->ts_scale,
        key_frame, lacing);

    if (lacing == 0) {
        // no extra info.
    } else if (lacing == 1) {
        // Xiph lacing.
    } else if (lacing == 2) {
        // fixed-size lacing.
    } else if (lacing == 3) {
        // EBML lacing.
    }

    if (track->is_video) {
        if (data_len >= 4 && lacing == 0) {
            uint32_t avcc_len = get_int32(data);
            printf("%s  first avcc len: %u\n", get_depth_space(ctx->depth), avcc_len);
        }
    } else if (track->is_audio) {
        // print several hex data.
        /*
        uint32_t print_len = data_len;
        if (print_len > 10) {
            print_len = 10;
        }
        printf("%saudio data.\n", get_depth_space(ctx->depth));
        print_hex(data, print_len, ctx->depth * 2, 0);
        */
    }

    return 0;
}
