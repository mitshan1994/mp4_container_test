#pragma once

#include <stdio.h>
#include <stdint.h>

// EBML Element types.
typedef enum element_type_t
{
    ELE_UNKNOWN,
    ELE_MASTER,
    ELE_BINARY,
    ELE_UTF8,
    ELE_ASCII,
    ELE_UINT,
    ELE_SINT,
    ELE_FLOAT,
    ELE_DATE
} element_type_t;

typedef struct mkv_element_t
{
    uint64_t id;
    uint64_t data_size;
    element_type_t type;
    const char *desc;
} mkv_element_t;

typedef struct mkv_track_t
{
    uint64_t id;

    int is_video;
    int is_audio;
    int is_subtitle;

    char codec_str[40]; // CodecID element STRING.

    double ts_scale;

} mkv_track_t;

typedef struct mkv_cluster_t
{
    uint64_t content_abs_pos;   // absolute position in file, excluding id and size.
    uint64_t timestamp;
} mkv_cluster_t;

typedef struct mkv_ctx_t {
    FILE *f;

    int32_t depth;

    uint64_t segment_start;
    uint64_t ts_scale;

    mkv_track_t *cur_track;
    mkv_track_t *tracks[40];
    uint32_t track_count;

    //uint64_t cur_cluster_ts;
    mkv_cluster_t *cur_cluster;
    mkv_cluster_t *clusters;
    uint32_t cluster_count;
    uint32_t cluster_capacity;
} mkv_ctx_t;
