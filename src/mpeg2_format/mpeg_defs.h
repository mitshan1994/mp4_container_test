#pragma once

#include <stdio.h>
#include <stdint.h>

// stream type
#define MPEG_ST_AAC   0x0f
#define MPEG_ST_AVC   0x1b
#define MPEG_ST_HEVC  0x24

typedef struct mpeg_stream_t
{
    uint16_t pid;
    uint16_t stream_type;
    uint8_t stream_id;  // in pes.
    char stream_str[16];

    int is_avc;
    int is_hevc;
    int is_aac;

    FILE *debug_pes_f;  // pes output for debug.
    FILE *debug_es_f;   // es output for debug.

    uint64_t pes_count; // statistics.

    uint8_t *pes_data;
    uint32_t pes_length;
    uint32_t pes_capacity;

} mpeg_stream_t;

typedef struct mpeg_ctx_t
{
    FILE *f;
    uint64_t file_size;

    // ts
    uint64_t ts_packet_count;

    uint16_t pmt_pid;
    uint16_t program_map_pid;

    mpeg_stream_t *streams[32];
    uint32_t stream_count;

} mpeg_ctx_t;